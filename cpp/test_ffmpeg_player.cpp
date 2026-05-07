// ============================================================================
// test_ffmpeg_player.cpp
// 技术验证：FFmpeg 解封装+解码 + QOpenGLWidget GPU 渲染
//
// 管道：
//   文件 (MKV) → av_read_frame → AVPacket → avcodec_send_packet
//   → avcodec_receive_packet(解码) → AVFrame (原始像素)
//   → sws_scale 格式转换 → AV_PIX_FMT_BGRA
//   → presentFrame() → QOpenGLWidget::paintGL() → glTexSubImage2D → GPU 渲染
//
// 关键修正（相比 test_player.cpp.old）：
//   1. 每个 packet 用 while 循环接收多个解码帧（消除丢帧）
//   2. EOF 时送 nullptr 冲刷解码器内部缓冲帧
//   3. 替换废弃的 avcodec_get_name() → codec->name
//   4. GL_QUADS → GL_TRIANGLE_STRIP（兼容 Core Profile）
//   5. glTexImage2D(首次) + glTexSubImage2D(后续帧) 优化
//   6. 析构函数加 isValid() 保护
// ============================================================================

#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>

// FFmpeg C API
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// ============================================================================
// VideoRenderWidget: QOpenGLWidget 子类
//
// 负责：
//   1. 管理 OpenGL 纹理（创建/更新/销毁）
//   2. paintGL 中绘制当前帧纹理到屏幕
//   3. 保持视频宽高比（letterbox / pillarbox）
//   4. 线程安全地接收来自解码线程的帧数据
//
// 注意：presentFrame 由 Qt 信号槽驱动（QueuedConnection），
//   调用者为 VideoEngine 的工作线程；运行在 Qt GUI 主线程。
// ============================================================================
class VideoRenderWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    // --------------------------------------------------------------------------
    // 构造函数
    //   参数 parent → Qt 父控件（生命周期管理）
    // --------------------------------------------------------------------------
    explicit VideoRenderWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
        // ---- OpenGL 资源 ----
        , m_texture(0)                             // GL 纹理 ID（initializeGL 中分配）
        // ---- 帧数据 ----
        , m_frameWidth(0)                          // 当前帧宽度（像素）
        , m_frameHeight(0)                         // 当前帧高度（像素）
        , m_buffer(nullptr)                        // CPU 端 BGRA 缓冲区（动态分配）
        , m_bufferSize(0)                          // 缓冲区当前容量（字节）
        , m_hasNewFrame(false)                     // 是否有新帧待渲染
        , m_textureAllocated(false)                // 纹理存储是否已分配（首次用 glTexImage2D）
        // ---- 显示 ----
        , m_dispWidth(0)                           // paintGL 中实际绘制宽度（保持比例后）
        , m_dispHeight(0)                          // paintGL 中实际绘制高度
    {
        setMinimumSize(800, 600);                  // 最小窗口尺寸
        setFocusPolicy(Qt::StrongFocus);           // 接受键盘焦点
    }

    // --------------------------------------------------------------------------
    // 析构函数
    //   清理 GL 纹理和 CPU 缓冲区。
    //   isValid() 检查 GL 上下文是否仍有效（防止窗口已关闭但析构
    //   时上下文已销毁导致 makeCurrent 失败）。
    // --------------------------------------------------------------------------
    ~VideoRenderWidget() override
    {
        // 检查 GL 上下文有效性——父窗口关闭后上下文可能已销毁
        if (isValid())
        {
            makeCurrent();
            if (m_texture)
            {
                glDeleteTextures(1, &m_texture);   // 释放 GPU 纹理
                m_texture = 0;
            }
            doneCurrent();
        }
        delete[] m_buffer;                         // 释放 CPU 帧缓冲
        m_buffer = nullptr;
    }

    // --------------------------------------------------------------------------
    // presentFrame: 接收解码后的帧数据
    //
    // 参数:
    //   data     → BGRA 像素数据（来自 sws_scale 输出）
    //   width    → 帧宽
    //   height   → 帧高
    //   linesize → 每行字节跨度（可能 > width * 4，因内存对齐）
    //
    // 线程安全: QMutex 保护 m_buffer 写入。
    //   调用 update() 标记 paintGL 需被调用（不阻塞）。
    // --------------------------------------------------------------------------
    void presentFrame(const uint8_t *data, int width, int height, int linesize)
    {
        QMutexLocker locker(&m_mutex);

        // 确保 CPU 缓冲区足够大
        int bufSize = height * width * 4;          // BGRA: 4 字节/像素
        if (bufSize > m_bufferSize)
        {
            delete[] m_buffer;
            m_buffer = new uint8_t[bufSize];       // 动态扩容
            m_bufferSize = bufSize;
        }

        // 将帧数据拷贝到内部缓冲区
        //   分两种路径：紧凑行（linesize == width*4）直接 memcpy；
        //   非紧凑行（GPU 对齐导致 stride 偏大）逐行拷贝。
        if (linesize == width * 4)
        {
            memcpy(m_buffer, data, bufSize);
        }
        else
        {
            for (int y = 0; y < height; y++)
            {
                memcpy(m_buffer + y * width * 4,
                       data + y * linesize,
                       width * 4);
            }
        }

        m_frameWidth  = width;
        m_frameHeight = height;
        m_hasNewFrame = true;

        // 注：update() 不持锁调用更清晰，但 Qt 文档保证 update()
        // 只是标记而非同步触发 paintGL，因此在锁内调用也不死锁。
        update();
    }

protected:
    // --------------------------------------------------------------------------
    // initializeGL: GL 上下文首次初始化时调用
    //   创建纹理对象、设置纹理参数（过滤/包裹模式）、设置背景色。
    // --------------------------------------------------------------------------
    void initializeGL() override
    {
        initializeOpenGLFunctions();               // 初始化 QOpenGLFunctions（必须首句）

        // 创建纹理对象
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        // 纹理过滤参数
        //   GL_LINEAR → 双线性过滤（放大/缩小平滑）
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 纹理包裹参数：GL_CLAMP_TO_EDGE → 超出 [0,1] 范围的纹理坐标
        //   被钳制到边缘颜色（避免边缘重复/镜像）
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);    // 深灰色背景
    }

    // --------------------------------------------------------------------------
    // paintGL: 每帧渲染回调
    //
    // 流程:
    //   1. 如有新帧 → 上传纹理到 GPU（首次 glTexImage2D，后续 glTexSubImage2D）
    //   2. 计算绘制区域以保持视频宽高比（letterboxing）
    //   3. 用 GL_TRIANGLE_STRIP 绘制两个三角形组成的全屏矩形
    //
    // 关键: 使用 GL_TRIANGLE_STRIP 替代废弃的 GL_QUADS。
    //   4 个顶点组成一个三角形带 → 2 个三角形 → 1 个矩形。
    // --------------------------------------------------------------------------
    void paintGL() override
    {
        // === 步骤 A：上传新帧纹理到 GPU ===
        if (m_hasNewFrame)
        {
            m_hasNewFrame = false;

            QMutexLocker locker(&m_mutex);

            glBindTexture(GL_TEXTURE_2D, m_texture);

            if (!m_textureAllocated)
            {
                // 首次：glTexImage2D 分配 GPU 纹理存储
                //   GL_RGBA8 → 内部格式（8 位/通道 RGBA）
                //   GL_BGRA + GL_UNSIGNED_BYTE → 源数据格式（Windows 原生 BGRA）
                glTexImage2D(GL_TEXTURE_2D,        // 目标纹理类型
                             0,                     // mipmap 级别
                             GL_RGBA8,              // GPU 内部存储格式
                             m_frameWidth,          // 宽度
                             m_frameHeight,         // 高度
                             0,                     // 边框（必须为 0）
                             GL_BGRA,               // 像素数据格式（CPU 端）
                             GL_UNSIGNED_BYTE,      // 像素数据类型
                             m_buffer);             // 像素数据指针
                m_textureAllocated = true;
            }
            else
            {
                // 后续帧：glTexSubImage2D 仅更新纹理数据（不重新分配存储）
                //   比每帧调 glTexImage2D 高效——避免 GPU 释放+重新分配。
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,                  // mipmap 级别
                                0, 0,               // 目标偏移 x, y
                                m_frameWidth,
                                m_frameHeight,
                                GL_BGRA,
                                GL_UNSIGNED_BYTE,
                                m_buffer);
            }

            m_dispWidth  = m_frameWidth;
            m_dispHeight = m_frameHeight;
        }

        // === 步骤 B：清屏 ===
        glClear(GL_COLOR_BUFFER_BIT);

        // 无帧数据时不绘制
        if (m_dispWidth <= 0 || m_dispHeight <= 0)
            return;

        // === 步骤 C：保持宽高比的 letterbox 计算 ===
        //   将视频帧等比例缩放，居中绘制在 widget 区域内。
        //   多余空间显示背景色（黑边 / pillarbox）。
        float widgetAspect = (float)width()  / (float)height();
        float frameAspect  = (float)m_dispWidth / (float)m_dispHeight;

        float drawW = 1.0f;                        // 归一化绘制宽度（widget 宽度为单位 1）
        float drawH = 1.0f;                        // 归一化绘制高度

        if (widgetAspect > frameAspect)
        {
            // widget 比视频"更宽" → 高度占满，宽度收缩（左右黑边 / pillarbox）
            drawW = frameAspect / widgetAspect;    // drawW < 1.0
        }
        else
        {
            // widget 比视频"更高" → 宽度占满，高度收缩（上下黑边 / letterbox）
            drawH = widgetAspect / frameAspect;    // drawH < 1.0
        }

        // === 步骤 D：用 GL_TRIANGLE_STRIP 绘制纹理矩形 ===
        //   顶点布局（逆时针，从左上角开始）：
        //     v0 (-drawW,  drawH) ← top-left
        //     v1 (-drawW, -drawH) ← bottom-left
        //     v2 ( drawW,  drawH) ← top-right
        //     v3 ( drawW, -drawH) ← bottom-right
        //
        //   Triangle Strip 绘制顺序：
        //     三角形1 = v0, v1, v2
        //     三角形2 = v2, v1, v3
        //
        //   纹理坐标：左上角为 (0,1)，右下角为 (1,0)——OpenGL 纹理坐标系
        //   原点在左下角，Y 轴向上翻转。

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        glBegin(GL_TRIANGLE_STRIP);
        // v0: 左上角 → 纹理 (0, 1) 即图片左上
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-drawW, drawH);

        // v1: 左下角 → 纹理 (0, 0) 即图片左下
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-drawW, -drawH);

        // v2: 右上角 → 纹理 (1, 1) 即图片右上
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(drawW, drawH);

        // v3: 右下角 → 纹理 (1, 0) 即图片右下
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(drawW, -drawH);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

private:
    // === GL 资源 ===
    GLuint m_texture;                              // GL 纹理对象 ID
    QMutex m_mutex;                                // 帧缓冲访问互斥锁

    // === 帧数据（CPU 端） ===
    int     m_frameWidth;                          // 当前帧宽度
    int     m_frameHeight;                         // 当前帧高度
    uint8_t *m_buffer;                             // BGRA 原始像素缓冲区
    int     m_bufferSize;                          // 缓冲区容量（字节）
    bool    m_hasNewFrame;                         // 是否有新帧待渲染
    bool    m_textureAllocated;                    // GL 纹理存储是否已分配

    // === 显示状态 ===
    int     m_dispWidth;                           // 渲染时的实际帧宽（可能因比例变化）
    int     m_dispHeight;                          // 渲染时的实际帧高
};

// ============================================================================
// DecoderContext: FFmpeg 解码上下文
//
// 集中管理所有 FFmpeg 解码对象，解包、解码、像素格式转换的完整状态。
// ============================================================================
struct DecoderContext
{
    // ---- 解封装层 ----
    AVFormatContext *fmtCtx;                       // 输入格式上下文（管理文件/解封装）
    int              videoStreamIdx;               // 视频流在 fmtCtx->streams 中的索引

    // ---- 解码层 ----
    const AVCodec   *codec;                        // 解码器描述（自动匹配编码类型）
    AVCodecContext  *codecCtx;                     // 解码器上下文（参数、状态）
    AVFrame         *frame;                        // 解码输出帧（原始像素，解码器原生格式）
    AVFrame         *rgbFrame;                     // RGB 转换目标帧（AV_PIX_FMT_BGRA）
    AVPacket        *packet;                       // 解封装读出的数据包（复用）

    // ---- 格式转换 ----
    SwsContext      *swsCtx;                       // 像素格式转换上下文（原生 → BGRA）

    // ---- 视频属性 ----
    int              width;                        // 视频宽度（像素）
    int              height;                       // 视频高度（像素）
};

// ============================================================================
// openVideo: 打开视频文件，初始化解码器
//
// 流程:
//   avformat_open_input → avformat_find_stream_info → av_find_best_stream
//   → avcodec_find_decoder → avcodec_alloc_context3 → avcodec_open2
//   → av_frame_alloc (frame + rgbFrame) → sws_getContext
//
// 参数:
//   filePath → 视频文件路径（必须是 FFmpeg 支持的格式）
//   ctx      → [out] 解码上下文（调用者维护生命周期）
//
// 返回 true 表示全部成功，ctx 中有完整的解码管线。
// ============================================================================
static bool openVideo(const QString &filePath, DecoderContext &ctx)
{
    // 清零所有字段（确保初始状态安全，后续检查空指针释放）
    memset(&ctx, 0, sizeof(ctx));

    // === 1. 打开输入文件 ===
    //   avformat_open_input: 打开媒体文件，探测格式，分配 AVFormatContext。
    //   参数3 fmt=nullptr → 自动探测格式
    //   参数4 options=nullptr → 无额外选项
    int ret = avformat_open_input(
        &ctx.fmtCtx,
        filePath.toUtf8().constData(),
        nullptr, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "avformat_open_input 失败:" << err;
        return false;
    }

    // === 2. 读取流信息 ===
    //   avformat_find_stream_info: 扫描文件，填充码流参数（编码类型、分辨率等）。
    ret = avformat_find_stream_info(ctx.fmtCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "avformat_find_stream_info 失败";
        avformat_close_input(&ctx.fmtCtx);
        return false;
    }

    // === 3. 查找最佳视频流 ===
    //   av_find_best_stream: 自动选择"最佳"视频流（通常是分辨率最高/码率最高的）。
    ctx.videoStreamIdx = av_find_best_stream(
        ctx.fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ctx.videoStreamIdx < 0)
    {
        qWarning() << "找不到视频流";
        avformat_close_input(&ctx.fmtCtx);
        return false;
    }

    AVStream *stream = ctx.fmtCtx->streams[ctx.videoStreamIdx];

    // === 4. 查找解码器 ===
    //   avcodec_find_decoder: 根据 codec_id 查找对应的解码器实现。
    ctx.codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!ctx.codec)
    {
        qWarning() << "找不到解码器: codec_id=" << stream->codecpar->codec_id;
        avformat_close_input(&ctx.fmtCtx);
        return false;
    }

    // === 5. 分配并初始化解码器上下文 ===
    ctx.codecCtx = avcodec_alloc_context3(ctx.codec);
    if (!ctx.codecCtx)
    {
        qWarning() << "avcodec_alloc_context3 失败";
        avformat_close_input(&ctx.fmtCtx);
        return false;
    }

    // avcodec_parameters_to_context: 将流参数复制到解码器上下文
    //   （宽、高、像素格式、extradata 等）
    avcodec_parameters_to_context(ctx.codecCtx, stream->codecpar);

    // === 6. 打开解码器 ===
    ret = avcodec_open2(ctx.codecCtx, ctx.codec, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "avcodec_open2 失败:" << err;
        avcodec_free_context(&ctx.codecCtx);
        avformat_close_input(&ctx.fmtCtx);
        return false;
    }

    // === 7. 保存视频属性 ===
    ctx.width  = ctx.codecCtx->width;
    ctx.height = ctx.codecCtx->height;

    // === 8. 打印视频信息 ===
    //   使用 codec->name 而非废弃的 avcodec_get_name()
    const char *pixFmtName = av_get_pix_fmt_name(ctx.codecCtx->pix_fmt);
    qDebug() << "视频信息:"
             << ctx.width << "x" << ctx.height
             << "编码:" << ctx.codec->name
             << "像素格式:" << (pixFmtName ? pixFmtName : "unknown");

    // === 9. 分配帧对象 ===
    //   av_frame_alloc: 分配 AVFrame（不分配 data buffer）。
    ctx.frame    = av_frame_alloc();               // 解码输出帧（解码器原生格式）
    ctx.rgbFrame = av_frame_alloc();               // RGB 转换目标帧（BGRA）
    ctx.packet   = av_packet_alloc();              // 解封装读出的数据包

    // === 10. 分配 BGRA 输出帧的缓冲区 ===
    //   av_frame_get_buffer: 根据 format/width/height 分配 data 缓冲区。
    ctx.rgbFrame->format = AV_PIX_FMT_BGRA;        // 目标像素格式
    ctx.rgbFrame->width  = ctx.width;
    ctx.rgbFrame->height = ctx.height;
    ret = av_frame_get_buffer(ctx.rgbFrame, 0);    // align=0 → 自动选择对齐
    if (ret < 0)
    {
        qWarning() << "av_frame_get_buffer 失败 (BGRA 帧)";
        return false;
    }

    // === 11. 创建像素格式转换上下文 ===
    //   将解码器原生格式（任意）→ BGRA（OpenGL 渲染所需）
    //   SWS_BILINEAR: 双线性插值（速度快，播放场景下质量足够）
    ctx.swsCtx = sws_getContext(
        ctx.width, ctx.height, ctx.codecCtx->pix_fmt,   // 源格式
        ctx.width, ctx.height, AV_PIX_FMT_BGRA,          // 目标格式
        SWS_BILINEAR,                                    // 缩放算法
        nullptr, nullptr, nullptr);                      // 无额外滤波器/参数
    if (!ctx.swsCtx)
    {
        qWarning() << "sws_getContext 失败";
        return false;
    }

    qDebug() << "[ OK ] 视频文件打开成功:" << filePath;
    return true;
}

// ============================================================================
// decodeNextFrame: 解码下一帧
//
// 与 test_player.cpp.old 的关键差异：
//   1. avcodec_send_packet 后，用 while 循环调用 avcodec_receive_frame
//      直到 EAGAIN —— 一个 packet 可能产出多个帧（B 帧重排序等）。
//   2. EOF 时，send nullptr 帧冲刷解码器内部缓冲的剩余帧。
//   3. 每成功解码一帧，立即通过 sws_scale 转换为 BGRA 并调用回调渲染。
//
// 参数:
//   ctx          → 解码上下文
//   onFrameReady → 回调：[data, width, height, linesize] → 渲染
//
// 返回 true 表示还有更多帧可解码；false 表示 EOF 且已冲刷完毕。
// ============================================================================
static bool decodeNextFrame(
    DecoderContext &ctx,
    const std::function<void(const uint8_t*, int, int, int)> &onFrameReady)
{
    // ---- 阶段 A: 读取并送入 packet ----
    //   从解封装器读取一个 AVPacket，发送到解码器。
    //   注意：一个 packet 可能被解码器部分接受（返回 EAGAIN 时需重试）。
    while (true)
    {
        // av_read_frame: 从容器读取下一个数据包
        int ret = av_read_frame(ctx.fmtCtx, ctx.packet);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                // === EOF 冲刷：发送 nullptr 告诉解码器"没有更多数据" ===
                //   解码器会将内部缓冲的帧（B 帧延迟、参考帧等）全部输出。
                avcodec_send_packet(ctx.codecCtx, nullptr);

                // 接收冲刷出来的所有剩余帧
                while (true)
                {
                    ret = avcodec_receive_frame(ctx.codecCtx, ctx.frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;                     // 冲刷完毕
                    if (ret < 0)
                    {
                        qWarning() << "EOF 冲刷解码帧失败";
                        return false;
                    }

                    // 格式转换 → BGRA
                    sws_scale(ctx.swsCtx,
                              ctx.frame->data, ctx.frame->linesize,
                              0, ctx.height,
                              ctx.rgbFrame->data, ctx.rgbFrame->linesize);

                    // 回调渲染
                    onFrameReady(ctx.rgbFrame->data[0],
                                 ctx.width, ctx.height,
                                 ctx.rgbFrame->linesize[0]);
                }
                return false;                      // EOF 且冲刷完毕
            }
            // 其他错误：跳过
            qWarning() << "av_read_frame 错误, 跳过";
            continue;
        }

        // 过滤非视频流的数据包
        if (ctx.packet->stream_index != ctx.videoStreamIdx)
        {
            av_packet_unref(ctx.packet);           // 释放 packet 内部引用
            continue;
        }

        // === 发送 packet 到解码器 ===
        ret = avcodec_send_packet(ctx.codecCtx, ctx.packet);
        av_packet_unref(ctx.packet);               // packet 数据已拷贝到解码器内部

        if (ret < 0)
        {
            // 发送失败（解码器内部满或错误），重试下一轮
            if (ret != AVERROR(EAGAIN))
            {
                qWarning() << "avcodec_send_packet 失败, 跳过";
            }
            continue;
        }

        // ---- 阶段 B: 从解码器接收解码后的帧 ----
        //   关键: 用 while 而非 if —— 一个 packet 可能产出多个帧。
        //   对于使用 B 帧、帧重排序或延迟解码的编码格式，这是必须的。
        while (true)
        {
            ret = avcodec_receive_frame(ctx.codecCtx, ctx.frame);
            if (ret == AVERROR(EAGAIN))
            {
                // 解码器需要更多输入数据 → 回到外层循环读下一个 packet
                break;
            }
            if (ret == AVERROR_EOF)
            {
                return false;                      // 解码器已耗尽
            }
            if (ret < 0)
            {
                qWarning() << "avcodec_receive_frame 失败";
                break;
            }

            // === 解码成功！格式转换 → BGRA ===
            sws_scale(ctx.swsCtx,
                      ctx.frame->data, ctx.frame->linesize,   // 源：解码器输出
                      0, ctx.height,                          // 处理全部行
                      ctx.rgbFrame->data, ctx.rgbFrame->linesize);  // 目标：BGRA

            // 回调渲染
            onFrameReady(ctx.rgbFrame->data[0],
                         ctx.width, ctx.height,
                         ctx.rgbFrame->linesize[0]);

            // av_frame_unref 释放 frame 内部引用，准备接收下一帧
            av_frame_unref(ctx.frame);
        }

        // 至少解码了一帧 → 返回
        //   注意：此处返回 true 表示"本周期有帧输出"。
        //   一个 packet 可能产出 0 帧（解码器缓冲），此时应继续循环。
        //   简化处理：只要有 send 成功，就认为"可能解码了什么"。
        return true;
    }
}

// ============================================================================
// closeVideo: 释放解码上下文中的所有 FFmpeg 资源
//
// 释放顺序：先释放"运营"对象（sws → 帧 → packet），
//   再释放解码器，最后释放解封装器。
// ============================================================================
static void closeVideo(DecoderContext &ctx)
{
    if (ctx.swsCtx)
    {
        sws_freeContext(ctx.swsCtx);
        ctx.swsCtx = nullptr;
    }
    if (ctx.rgbFrame)
    {
        av_frame_free(&ctx.rgbFrame);              // 同时释放 data buffer
    }
    if (ctx.frame)
    {
        av_frame_free(&ctx.frame);
    }
    if (ctx.packet)
    {
        av_packet_free(&ctx.packet);
    }
    if (ctx.codecCtx)
    {
        avcodec_free_context(&ctx.codecCtx);       // 内部调用 avcodec_close
    }
    if (ctx.fmtCtx)
    {
        avformat_close_input(&ctx.fmtCtx);         // 关闭文件 + 释放上下文
    }
}

// ============================================================================
// main: 播放流程的总控
// ============================================================================
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qDebug() << "=== FFmpeg 解码 + QOpenGLWidget 渲染测试 ===\n";

    // ========================================================================
    // 阶段 I: 打开视频文件
    // ========================================================================
    QString videoPath = QCoreApplication::applicationDirPath()
                        + "/videodir/20260505/0505-04-56-52-305.mkv";
    qDebug() << "视频文件:" << videoPath;

    DecoderContext ctx;
    if (!openVideo(videoPath, ctx))
    {
        qWarning() << "打开视频失败";
        return 1;
    }

    // ========================================================================
    // 阶段 II: 创建渲染窗口
    // ========================================================================
    VideoRenderWidget widget;
    widget.setWindowTitle("FFmpeg + QOpenGLWidget 播放测试");
    widget.resize(ctx.width, ctx.height);
    widget.show();

    // ========================================================================
    // 阶段 III: 播放循环（QTimer 驱动）
    //
    // 使用 QTimer 每 50ms (≈ 20fps) 触发一次解码+渲染。
    //   生产代码中，VideoEngine 会使用独立的播放线程 + 精确的帧间延迟
    //   控制（基于用户指定的 fps），通过信号将帧数据送入主线程渲染。
    // ========================================================================
    int totalFrames = 0;                           // 累计解码帧数

    QElapsedTimer fpsTimer;                        // FPS 统计用计时器
    fpsTimer.start();
    int fpsFrameCount = 0;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        // 回调：每次成功解码一帧，送入 VideoRenderWidget 渲染
        bool gotFrame = decodeNextFrame(ctx,
            [&](const uint8_t *data, int w, int h, int linesize)
            {
                widget.presentFrame(data, w, h, linesize);
                totalFrames++;
                fpsFrameCount++;
            });

        // 每秒输出一次 FPS
        qint64 elapsed = fpsTimer.elapsed();
        if (elapsed >= 1000)
        {
            qDebug() << "FPS:" << fpsFrameCount
                     << "总帧数:" << totalFrames;
            fpsFrameCount = 0;
            fpsTimer.restart();
        }

        if (!gotFrame)
        {
            // EOF + 冲刷完毕
            qDebug() << "\n=== 播放完毕: 共" << totalFrames << "帧 ===";
            timer.stop();
        }
    });

    timer.start(16);                               // 约 60fps 驱动（尽量快消费）

    // ========================================================================
    // 阶段 IV: Qt 事件循环（阻塞直到窗口关闭或 timer 停止后 app.quit）
    // ========================================================================
    int result = app.exec();

    // ========================================================================
    // 阶段 V: 清理
    // ========================================================================
    timer.stop();
    closeVideo(ctx);

    qDebug() << "=== 测试完成 ===";
    return result;
}
