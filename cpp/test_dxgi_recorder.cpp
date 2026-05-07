// ============================================================================
// test_dxgi_recorder.cpp
// 技术验证：DXGI Desktop Duplication 屏幕捕获 + FFmpeg FFV1 无损编码 + MKV 封装
//
// 管道：
//   DXGI AcquireNextFrame → GPU 桌面纹理 (全屏, BGRA)
//   → ID3D11DeviceContext::CopySubresourceRegion (裁剪 800×600, BGRA)
//   → Staging 纹理 Map/Unmap → CPU 系统内存 (BGRA 原始数据)
//   → avcodec_send_frame + avcodec_receive_packet → FFV1 编码 (BGRA 直送)
//   → av_interleaved_write_frame → MKV 文件封装
//
// 说明：
//   FFV1 编码器原生支持 bgra / bgr0 像素格式，无需像素格式转换。
//   DXGI 输出的 BGRA 数据直接送入 FFV1 编码器，零拷贝、零损失。
// ============================================================================

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

// FFmpeg C API
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

// ============================================================================
// 常量定义
// ============================================================================

// 录制目标尺寸（需求规格：800×600 固定）
static const UINT RECORD_WIDTH  = 800;
static const UINT RECORD_HEIGHT = 600;

// 录制帧率（需求规格：20fps）
static const int RECORD_FPS = 20;

// AcquireNextFrame 超时毫秒
static const UINT ACQUIRE_TIMEOUT_MS = 500;

// 录制时长（秒），测试用
static const int RECORD_DURATION_SEC = 5;

// 输出文件名
static const char *OUTPUT_FILENAME = "test_dxgi_recording.mkv";

// ============================================================================
// 步骤 1：创建 D3D11 设备与设备上下文
//
// D3D11 设备是 GPU 操作的入口。需要 D3D_FEATURE_LEVEL_11_0 以上以支持
// Desktop Duplication API。DeviceContext 用于提交 GPU 命令。
// ============================================================================
static bool createD3D11Device(
    ID3D11Device        **outDevice,       // [out] D3D11 设备
    ID3D11DeviceContext **outContext)      // [out] 立即上下文（提交 GPU 命令）
{
    // 尝试的特性级别：优先 11.1，可降级到 11.0
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL selectedLevel;       // 实际选中的特性级别

    // D3D11CreateDevice: 创建 D3D11 设备和上下文
    //   参数1 adapter=nullptr → 使用默认显示适配器
    //   参数2 DriverType=HARDWARE → 使用硬件 GPU（非软件光栅化）
    //   参数3 Software=nullptr → 非软件驱动
    //   参数4 Flags=0 → 默认行为（调试时可加 D3D11_CREATE_DEVICE_DEBUG）
    //   参数5 featureLevels → 尝试的特性级别数组
    //   参数6 2 → featureLevels 数组长度
    //   参数7 SDK_VERSION → D3D11 SDK 版本标识
    HRESULT hr = D3D11CreateDevice(
        nullptr,                            // 默认适配器
        D3D_DRIVER_TYPE_HARDWARE,           // 硬件加速
        nullptr,                            // 无软件光栅化模块
        0,                                  // 创建标志
        featureLevels,                      // 尝试的特性级别
        2,                                  // 特性级别数量
        D3D11_SDK_VERSION,                  // SDK 版本
        outDevice,                          // 输出：ID3D11Device 指针
        &selectedLevel,                     // 输出：实际选中的特性级别
        outContext);                        // 输出：ID3D11DeviceContext 指针

    if (FAILED(hr))
    {
        printf("[FAIL] D3D11CreateDevice: HRESULT=0x%08lX\n", hr);
        return false;
    }

    printf("[ OK ] D3D11 设备创建成功 (Feature Level: 0x%04X)\n", selectedLevel);
    return true;
}

// ============================================================================
// 步骤 2-4：获取 DXGI 适配器 → 枚举输出 → 创建 IDXGIOutputDuplication
//
// 从 D3D11 设备出发：
//   1. QueryInterface 获取 IDXGIDevice
//   2. GetAdapter 获取 IDXGIAdapter（显卡）
//   3. EnumOutputs 枚举报每个输出（显示器）
//   4. QueryInterface IDXGIOutput1 → DuplicateOutput 创建桌面复制接口
// ============================================================================

// --------------------------------------------------------------------------
// 辅助：从 IDXGIOutput 创建 IDXGIOutputDuplication
// --------------------------------------------------------------------------
static bool createDesktopDuplication(
    IDXGIOutput                 *output,           // DXGI 输出（某台显示器）
    ID3D11Device                *d3dDevice,        // D3D11 设备
    IDXGIOutputDuplication     **outDup)           // [out] 桌面复制接口
{
    // 步骤 A：获取 IDXGIOutput1 接口（Win8+，Duplication 需要此接口）
    IDXGIOutput1 *output1 = nullptr;
    HRESULT hr = output->QueryInterface(
        __uuidof(IDXGIOutput1),                    // IDXGIOutput1 的 GUID
        (void **)&output1);
    if (FAILED(hr))
    {
        printf("[WARN] 此输出不支持 IDXGIOutput1 (需要 Win8+)\n");
        return false;
    }

    // 步骤 B：创建桌面复制对象
    //   DuplicateOutput 将桌面画面复制为可捕获的 GPU 纹理
    //   参数1 d3dDevice → 必须与创建输出的 D3D 设备一致
    hr = output1->DuplicateOutput(d3dDevice, outDup);
    output1->Release();                            // 用完即释放

    if (hr == E_ACCESSDENIED)
    {
        printf("[WARN] DuplicateOutput 被拒绝 (需非管理员运行，桌面须活动)\n");
        return false;
    }
    else if (hr == E_NOTIMPL)
    {
        printf("[WARN] DuplicateOutput 不支持 (可能是基本显示驱动)\n");
        return false;
    }
    else if (FAILED(hr))
    {
        printf("[WARN] DuplicateOutput 失败: HRESULT=0x%08lX\n", hr);
        return false;
    }

    return true;
}

// --------------------------------------------------------------------------
// 枚举所有输出，尝试为第一个成功的输出创建 Duplication
//   返回 true 表示找到并成功创建
//   注意：调用者负责通过 outDup 和 outOutputDesc 使用结果
// --------------------------------------------------------------------------
static bool initDuplicationOnFirstOutput(
    ID3D11Device            *d3dDevice,            // D3D11 设备
    IDXGIAdapter            *adapter,              // DXGI 适配器
    IDXGIOutputDuplication **outDup,               // [out] 桌面复制接口
    DXGI_OUTPUT_DESC        *outOutputDesc)        // [out] 输出描述（含尺寸等）
{
    IDXGIOutput *output = nullptr;

    // 遍历所有输出（i = 0, 1, 2, … 直到 EnumOutputs 返回 DXGI_ERROR_NOT_FOUND）
    for (UINT i = 0; ; i++)
    {
        HRESULT hr = adapter->EnumOutputs(i, &output);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;                                 // 已遍历完所有输出
        if (FAILED(hr))
            continue;                              // 单个输出枚举失败，继续尝试

        // 获取输出描述：名称、桌面坐标、旋转、是否主显示器等
        output->GetDesc(outOutputDesc);

        printf("[INFO] 输出 %u: \"%S\" (%ld x %ld) 旋转=%u\n",
               i,
               outOutputDesc->DeviceName,
               outOutputDesc->DesktopCoordinates.right
                   - outOutputDesc->DesktopCoordinates.left,
               outOutputDesc->DesktopCoordinates.bottom
                   - outOutputDesc->DesktopCoordinates.top,
               outOutputDesc->Rotation);

        // 尝试为此输出创建 Duplication
        if (createDesktopDuplication(output, d3dDevice, outDup))
        {
            printf("[ OK ] 输出 %u: Duplication 创建成功\n", i);
            output->Release();
            return true;
        }

        output->Release();
    }

    printf("[FAIL] 所有输出均无法创建 Duplication\n");
    return false;
}

// ============================================================================
// 步骤 5：创建 Staging 纹理用于 GPU→CPU 数据回读
//
// Desktop Duplication 返回的纹理是 D3D11_USAGE_DEFAULT（仅 GPU 可读写），
// CPU 无法直接访问。需要创建一个相同格式、尺寸的 staging 纹理
//（CPU_ACCESS_READ + USAGE_STAGING），通过 CopySubresourceRegion 将
// GPU 纹理数据拷贝到 staging 纹理，再 Map 到 CPU 可读指针。
// ============================================================================
static bool createStagingTexture(
    ID3D11Device   *d3dDevice,                     // D3D11 设备
    UINT            width,                         // 纹理宽度
    UINT            height,                        // 纹理高度
    DXGI_FORMAT     format,                        // 像素格式（应为 B8G8R8A8_UNORM）
    ID3D11Texture2D **outStaging)                  // [out] staging 纹理
{
    // D3D11_TEXTURE2D_DESC: 描述 2D 纹理的属性
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width              = width;
    stagingDesc.Height             = height;
    stagingDesc.MipLevels          = 1;            // 无 mipmap 链
    stagingDesc.ArraySize          = 1;            // 单纹理（非纹理数组）
    stagingDesc.Format             = format;       // 与源纹理相同格式
    stagingDesc.SampleDesc.Count   = 1;            // 无多重采样
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage              = D3D11_USAGE_STAGING;    // staging：CPU 可访问
    stagingDesc.BindFlags          = 0;            // 不绑定到管线阶段
    stagingDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;  // CPU 只读
    stagingDesc.MiscFlags          = 0;

    HRESULT hr = d3dDevice->CreateTexture2D(
        &stagingDesc,                              // 纹理描述
        nullptr,                                   // 无初始数据
        outStaging);                               // 输出纹理指针

    if (FAILED(hr))
    {
        printf("[FAIL] 创建 staging 纹理失败: HRESULT=0x%08lX\n", hr);
        return false;
    }

    printf("[ OK ] Staging 纹理创建成功 (%u × %u)\n", width, height);
    return true;
}

// ============================================================================
// 步骤 6：从桌面帧纹理读取 800×600 BGRA 数据到 CPU 内存
//
// 使用 D3D11_BOX 指定裁剪区域，CopySubresourceRegion 只拷贝目标区域到
// staging 纹理，然后 Map 读出。
// ============================================================================
static bool readFrameData(
    ID3D11DeviceContext *context,                  // D3D 设备上下文
    ID3D11Resource      *srcTexture,               // 源纹理 (GPU, 桌面帧)
    ID3D11Texture2D     *stagingTexture,           // 目标 staging 纹理
    uint8_t             *outBuffer)                // [out] CPU 缓冲区 (尺寸需 ≥ 800×600×4)
{
    // D3D11_BOX：定义 3D 子资源区域的拷贝范围
    //   left/top/front → 起始坐标（含）
    //   right/bottom/back → 结束坐标（不含）
    //   此处定义左上角 800×600 矩形区域
    D3D11_BOX srcBox;
    srcBox.left   = 0;
    srcBox.top    = 0;
    srcBox.front  = 0;
    srcBox.right  = RECORD_WIDTH;
    srcBox.bottom = RECORD_HEIGHT;
    srcBox.back   = 1;

    // CopySubresourceRegion: 将源纹理的指定区域拷贝到目标纹理的指定位置
    //   参数1 pDstResource → 目标资源 (staging 纹理)
    //   参数2 DstSubresource → 目标子资源索引
    //   参数3 DstX, DstY, DstZ → 目标纹理中的起始坐标
    //   参数4 pSrcResource → 源资源 (桌面帧纹理)
    //   参数5 SrcSubresource → 源子资源索引
    //   参数6 pSrcBox → 源区域边界（nullptr 表示整个纹理，此处指定 800×600）
    context->CopySubresourceRegion(
        stagingTexture,                            // 目标：staging 纹理
        0,                                         // 目标 mip/subresource
        0, 0, 0,                                   // DstX, DstY, DstZ
        srcTexture,                                // 源：桌面帧纹理
        0,                                         // 源 mip/subresource
        &srcBox);                                  // 拷贝区域：800×600 左上角

    // Map: 将 GPU staging 纹理映射到 CPU 可访问的地址空间
    //   MapType=D3D11_MAP_READ → 只读映射
    //   MapFlags=0 → 无特殊标志
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(
        stagingTexture,                            // staging 纹理
        0,                                         // 子资源索引
        D3D11_MAP_READ,                            // 只读映射
        0,                                         // 无标志
        &mapped);                                  // 输出：映射信息（pData, RowPitch, DepthPitch）
    if (FAILED(hr))
    {
        printf("[WARN] Map staging 纹理失败: HRESULT=0x%08lX\n", hr);
        return false;
    }

    // RowPitch 是 GPU 纹理每行的字节跨度（可能大于 width * 4，
    // 因为 GPU 纹理有对齐要求）。需要逐行拷贝以正确处理对齐差异。
    const UINT rowBytes = RECORD_WIDTH * 4;        // 800 × 4 字节/像素(BGRA) = 3200 字节/行

    for (UINT y = 0; y < RECORD_HEIGHT; y++)
    {
        memcpy(
            outBuffer + y * rowBytes,              // 目标：紧凑排列
            (uint8_t *)mapped.pData + y * mapped.RowPitch,  // 源：GPU 对齐排列
            rowBytes);                             // 每行拷贝 3200 字节
    }

    // Unmap: 解除 CPU 映射
    context->Unmap(stagingTexture, 0);

    return true;
}

// ============================================================================
// 步骤 7：初始化 FFmpeg 编码器 (FFV1) 与封装器 (MKV)
//
// FFV1 是 FFmpeg 的无损视频编码器，支持 GBRP（planar RGB）。
// MKV (Matroska) 是通用多媒体容器，支持 FFV1 编码流。
// ============================================================================

// --------------------------------------------------------------------------
// FFmpeg 编码上下文结构体：集中管理所有 FFmpeg 对象
// --------------------------------------------------------------------------
struct EncoderContext
{
    // ---- 封装层 ----
    AVFormatContext *fmtCtx;                       // 输出格式上下文（管理文件/封装）
    AVStream        *videoStream;                  // 视频流指针

    // ---- 编码层 ----
    const AVCodec   *codec;                        // 编码器描述（FFV1）
    AVCodecContext  *codecCtx;                     // 编码器上下文（参数、状态）
    AVPacket        *packet;                       // 编码输出数据包（复用，避免每帧分配）

    // ---- 帧处理 ----
    AVFrame         *frame;                        // 编码帧（BGRA，直接来自 DXGI）

    // ---- 时间戳 ----
    int64_t          frameIndex;                   // 帧序号（PTS 计数依据）
};

// --------------------------------------------------------------------------
// 初始化 FFmpeg 编码器 (FFV1) 与 MKV 封装
// --------------------------------------------------------------------------
static bool initFFmpegEncoder(EncoderContext &ctx, const char *filename)
{
    // 将结构体所有字段清零（memset 0 确保初始状态安全）
    memset(&ctx, 0, sizeof(ctx));

    // === 7.1 创建输出格式上下文 (MKV) ===
    //   avformat_alloc_output_context2 自动根据文件扩展名选择封装格式
    int ret = avformat_alloc_output_context2(
        &ctx.fmtCtx,                               // [out] 格式上下文
        nullptr,                                   // oformat=nullptr → 从扩展名推断
        nullptr,                                   // format_name=nullptr
        filename);                                 // 文件名（.mkv → Matroska）
    if (ret < 0 || !ctx.fmtCtx)
    {
        printf("[FAIL] 创建 MKV 输出上下文失败\n");
        return false;
    }

    // === 7.2 查找 FFV1 编码器 ===
    //   AV_CODEC_ID_FFV1: FFmpeg 无损视频编码器
    ctx.codec = avcodec_find_encoder(AV_CODEC_ID_FFV1);
    if (!ctx.codec)
    {
        printf("[FAIL] 找不到 FFV1 编码器 (你的 FFmpeg 编译时可能未包含 FFV1)\n");
        return false;
    }
    printf("[ OK ] 编码器: %s\n", ctx.codec->name);

    // === 7.3 创建视频流 ===
    //   avformat_new_stream: 在输出容器中创建一条新流
    ctx.videoStream = avformat_new_stream(ctx.fmtCtx, nullptr);
    if (!ctx.videoStream)
    {
        printf("[FAIL] 创建视频流失败\n");
        return false;
    }
    ctx.videoStream->id = ctx.fmtCtx->nb_streams - 1;  // 流 ID = 流索引

    // === 7.4 分配编码器上下文 ===
    ctx.codecCtx = avcodec_alloc_context3(ctx.codec);
    if (!ctx.codecCtx)
    {
        printf("[FAIL] 分配编码器上下文失败\n");
        return false;
    }

    // === 7.5 设置编码参数 ===
    //   说明：FFV1 原生支持 bgra / bgr0（packed RGB），无需格式转换。
    //   DXGI 输出的 BGRA 直接送入编码器，零损失。
    ctx.codecCtx->width      = RECORD_WIDTH;       // 帧宽度
    ctx.codecCtx->height     = RECORD_HEIGHT;      // 帧高度
    ctx.codecCtx->pix_fmt    = AV_PIX_FMT_BGRA;    // BGRA: FFV1 原生支持的 packed RGB
    ctx.codecCtx->time_base  = { 1, RECORD_FPS };  // 时基: 1/20 秒 = 每帧间隔
    ctx.codecCtx->framerate  = { RECORD_FPS, 1 };  // 帧率: 20 fps

    // 将编码器时间基复制到流
    ctx.videoStream->time_base = ctx.codecCtx->time_base;

    // 将编码器参数复制到流的 codecpar（某些封装器需要）
    avcodec_parameters_from_context(ctx.videoStream->codecpar, ctx.codecCtx);

    // === 7.6 打开编码器 ===
    //   avcodec_open2 初始化编码器内部状态
    ret = avcodec_open2(ctx.codecCtx, ctx.codec, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        printf("[FAIL] 打开编码器失败: %s\n", err);
        return false;
    }

    // === 7.7 打开输出文件 ===
    //   avio_open: 打开输出 IO 上下文以写入文件（非流式场景）
    if (!(ctx.fmtCtx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx.fmtCtx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char err[256];
            av_strerror(ret, err, sizeof(err));
            printf("[FAIL] 无法打开输出文件: %s\n", err);
            return false;
        }
    }

    // === 7.8 写入文件头 ===
    //   avformat_write_header: 向文件写入容器头信息（编码参数、轨道信息等）
    ret = avformat_write_header(ctx.fmtCtx, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        printf("[FAIL] 写入 MKV 文件头失败: %s\n", err);
        return false;
    }

    // === 7.9 分配编码帧 ===
    //   单帧 BGRA：DXGI 数据直接填充，无需转换
    ctx.frame = av_frame_alloc();
    ctx.frame->format = AV_PIX_FMT_BGRA;
    ctx.frame->width  = RECORD_WIDTH;
    ctx.frame->height = RECORD_HEIGHT;
    av_frame_get_buffer(ctx.frame, 0);
    ctx.packet = av_packet_alloc();

    ctx.frameIndex = 0;

    printf("[ OK ] FFmpeg 编码器初始化完成: FFV1 %dx%d @ %dfps → %s\n",
           RECORD_WIDTH, RECORD_HEIGHT, RECORD_FPS, filename);
    return true;
}

// --------------------------------------------------------------------------
// 编码一帧：将 BGRA 数据送入 FFV1 编码器，封装进 MKV
//
// 注意：avcodec_send_frame + avcodec_receive_packet 不是 1:1 映射。
//   某些编码器（如 FFV1）单帧可能产出 0 或 1 个 packet。
//   某些编码器（如 H.264）单帧可能产出 0 个或多个 packet（延迟）。
//   因此需要用 while 循环消费 receive 直到 EAGAIN。
// --------------------------------------------------------------------------
static bool encodeFrame(EncoderContext &ctx, const uint8_t *bgraData)
{
    // === 7.10a 将 BGRA 数据直接填充到编码帧 ===
    //   FFV1 原生支持 BGRA，无需格式转换
    memcpy(ctx.frame->data[0], bgraData,
           RECORD_WIDTH * RECORD_HEIGHT * 4);

    // === 7.10b 设置帧 PTS（展示时间戳） ===
    ctx.frame->pts = ctx.frameIndex;
    ctx.frameIndex++;

    // === 7.10c 送帧到编码器 ===
    int ret = avcodec_send_frame(ctx.codecCtx, ctx.frame);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        printf("[WARN] avcodec_send_frame 失败: %s\n", err);
        return false;
    }

    // === 7.10e 从编码器拉取编码后的数据包 ===
    //   while 循环：一个输入帧可能产出多个输出包（某些编码器需要）
    //   对 FFV1 而言通常单帧产单包，但仍须按标准模式处理
    while (ret >= 0)
    {
        av_packet_unref(ctx.packet);               // 重置 packet（释放内部 buffer 引用）

        ret = avcodec_receive_packet(ctx.codecCtx, ctx.packet);
        if (ret == AVERROR(EAGAIN))
        {
            break;                                 // 编码器需要更多输入帧，跳出循环
        }
        else if (ret == AVERROR_EOF)
        {
            break;                                 // 编码器已冲刷完毕
        }
        else if (ret < 0)
        {
            char err[256];
            av_strerror(ret, err, sizeof(err));
            printf("[WARN] avcodec_receive_packet 失败: %s\n", err);
            return false;
        }

        // === 7.10f 写入 MKV 容器 ===
        //   将 packet 的 stream_index 重映射到输出流的索引
        av_packet_rescale_ts(ctx.packet,
                             ctx.codecCtx->time_base,      // 源时基
                             ctx.videoStream->time_base);  // 目标时基
        ctx.packet->stream_index = ctx.videoStream->index;

        ret = av_interleaved_write_frame(ctx.fmtCtx, ctx.packet);
        if (ret < 0)
        {
            char err[256];
            av_strerror(ret, err, sizeof(err));
            printf("[WARN] 写入 MKV 帧失败: %s\n", err);
            return false;
        }
    }

    return true;
}

// --------------------------------------------------------------------------
// 冲刷编码器：发送 nullptr 帧以获取编码器内部缓冲的剩余数据包
//
// 必须在所有帧编码完成后调用，以将编码器内部缓冲的延迟帧写入文件。
// --------------------------------------------------------------------------
static bool flushEncoder(EncoderContext &ctx)
{
    // 发送 nullptr 表示"没有更多帧，请冲刷内部缓冲"
    int ret = avcodec_send_frame(ctx.codecCtx, nullptr);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        printf("[WARN] 冲刷编码器 send_frame 失败: %s\n", err);
        return false;
    }

    // 循环拉取所有剩余的编码数据包
    while (ret >= 0)
    {
        av_packet_unref(ctx.packet);

        ret = avcodec_receive_packet(ctx.codecCtx, ctx.packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
        {
            char err[256];
            av_strerror(ret, err, sizeof(err));
            printf("[WARN] 冲刷编码器 receive_packet 失败: %s\n", err);
            return false;
        }

        av_packet_rescale_ts(ctx.packet,
                             ctx.codecCtx->time_base,
                             ctx.videoStream->time_base);
        ctx.packet->stream_index = ctx.videoStream->index;

        ret = av_interleaved_write_frame(ctx.fmtCtx, ctx.packet);
        if (ret < 0)
        {
            char err[256];
            av_strerror(ret, err, sizeof(err));
            printf("[WARN] 冲刷写入帧失败: %s\n", err);
            return false;
        }
    }

    return true;
}

// --------------------------------------------------------------------------
// 资源释放：按逆序释放所有 FFmpeg 对象
// --------------------------------------------------------------------------
static void closeEncoder(EncoderContext &ctx)
{
    // 写入文件尾（关闭容器，修正 duration 和 index）
    if (ctx.fmtCtx)
    {
        av_write_trailer(ctx.fmtCtx);
    }

    // 释放顺序：帧 → packet → 编码器 → IO → 格式上下文
    if (ctx.frame)        { av_frame_free(&ctx.frame); }
    if (ctx.packet)        { av_packet_free(&ctx.packet); }
    if (ctx.codecCtx)      { avcodec_free_context(&ctx.codecCtx); }
    if (ctx.fmtCtx)
    {
        // 关闭 IO（如果不是 AVFMT_NOFILE 模式）
        if (ctx.fmtCtx->pb)
        {
            avio_closep(&ctx.fmtCtx->pb);
        }
        avformat_free_context(ctx.fmtCtx);
    }
}

// ============================================================================
// main: 录制流程的总控
// ============================================================================
int main()
{
    printf("=== DXGI 桌面捕获 + FFmpeg FFV1 录制测试 ===\n\n");

    // ========================================================================
    // 阶段 I: 初始化 DXGI Desktop Duplication
    // ========================================================================

    // ---- 1. 创建 D3D11 设备 ----
    ID3D11Device        *d3dDevice  = nullptr;
    ID3D11DeviceContext *d3dContext = nullptr;
    if (!createD3D11Device(&d3dDevice, &d3dContext))
    {
        return 1;
    }

    // ---- 2. 获取 DXGI 设备（IDXGIDevice） ----
    //   D3D 设备通过 COM 接口查询暴露 DXGI 功能
    IDXGIDevice *dxgiDevice = nullptr;
    HRESULT hr = d3dDevice->QueryInterface(
        __uuidof(IDXGIDevice),                     // DXGI 设备接口 GUID
        (void **)&dxgiDevice);
    if (FAILED(hr))
    {
        printf("[FAIL] 获取 IDXGIDevice 失败: HRESULT=0x%08lX\n", hr);
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }

    // ---- 3. 获取 DXGI 适配器（显卡） ----
    IDXGIAdapter *adapter = nullptr;
    hr = dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();                         // 不再需要 IDXGIDevice
    if (FAILED(hr))
    {
        printf("[FAIL] GetAdapter 失败: HRESULT=0x%08lX\n", hr);
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }

    // ---- 4. 枚举输出 + 创建 Desktop Duplication ----
    IDXGIOutputDuplication *dup        = nullptr;
    DXGI_OUTPUT_DESC        outputDesc = {};
    if (!initDuplicationOnFirstOutput(d3dDevice, adapter, &dup, &outputDesc))
    {
        adapter->Release();
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }
    adapter->Release();                            // 不再需要适配器

    // ---- 5. 创建 Staging 纹理 (800×600, BGRA) ----
    ID3D11Texture2D *stagingTexture = nullptr;
    if (!createStagingTexture(d3dDevice, RECORD_WIDTH, RECORD_HEIGHT,
                              DXGI_FORMAT_B8G8R8A8_UNORM,  // BGRA
                              &stagingTexture))
    {
        dup->Release();
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }

    // ========================================================================
    // 阶段 II: 初始化 FFmpeg 编码器与封装器
    // ========================================================================
    EncoderContext encCtx;
    if (!initFFmpegEncoder(encCtx, OUTPUT_FILENAME))
    {
        stagingTexture->Release();
        dup->Release();
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }

    // ========================================================================
    // 阶段 III: 录制循环
    // ========================================================================

    // CPU 端帧缓冲区：800×600 像素 × 4 字节(BGRA) = 1,920,000 字节
    const size_t frameBufferSize = RECORD_WIDTH * RECORD_HEIGHT * 4;
    uint8_t *frameBuffer = new uint8_t[frameBufferSize];

    // 记录开始时间，控制录制时长
    DWORD startTick = GetTickCount();
    DWORD endTick   = startTick + RECORD_DURATION_SEC * 1000;

    int totalFrames = 0;                           // 成功编码的帧数
    int capAttempts = 0;                           // 捕获尝试次数（含超时）

    printf("[ OK ] 开始录制，目标 %d 秒 @ %d fps → %s\n",
           RECORD_DURATION_SEC, RECORD_FPS, OUTPUT_FILENAME);
    printf("       尺寸: %u×%u | 等待桌面变化…\n", RECORD_WIDTH, RECORD_HEIGHT);

    // ---- 录制主循环 ----
    while (GetTickCount() < endTick)
    {
        // === III.a 获取桌面帧 ===
        IDXGIResource           *frameResource = nullptr;  // 获取到的帧资源（GPU 端）
        DXGI_OUTDUPL_FRAME_INFO  frameInfo = {};           // 帧元数据（时间戳、脏矩形等）

        // AcquireNextFrame: 等待下一帧桌面变化
        //   参数1 TimeoutInMilliseconds → 超时
        //   参数2 pFrameInfo → 帧元数据输出
        //   参数3 ppDesktopResource → 帧资源输出（IDXGIResource）
        hr = dup->AcquireNextFrame(ACQUIRE_TIMEOUT_MS, &frameInfo, &frameResource);
        capAttempts++;

        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            // 桌面无变化 → 这是正常行为，继续轮询
            continue;
        }
        else if (hr == DXGI_ERROR_ACCESS_LOST)
        {
            // 访问丢失（UAC 弹窗、Ctrl+Alt+Del、全屏切换等）
            //   生产环境需重建整个 Duplication 链，测试环境直接退出
            printf("[WARN] DXGI_ERROR_ACCESS_LOST — 桌面访问丢失\n");
            break;
        }
        else if (FAILED(hr))
        {
            printf("[WARN] AcquireNextFrame 失败: HRESULT=0x%08lX\n", hr);
            if (frameResource) frameResource->Release();
            continue;
        }

        // === III.b 查询帧纹理（从 IDXGIResource 获取 ID3D11Texture2D） ===
        ID3D11Texture2D *desktopTexture = nullptr;
        hr = frameResource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            (void **)&desktopTexture);
        if (FAILED(hr))
        {
            printf("[WARN] 获取帧纹理失败: HRESULT=0x%08lX\n", hr);
            dup->ReleaseFrame();                   // 释放帧后才能继续 AcquireNextFrame
            frameResource->Release();
            continue;
        }

        // === III.c GPU→CPU 回读：裁剪 800×600 BGRA 到 frameBuffer ===
        if (readFrameData(d3dContext, desktopTexture, stagingTexture, frameBuffer))
        {
            // === III.d 编码 + 封装 ===
            if (encodeFrame(encCtx, frameBuffer))
            {
                totalFrames++;
            }
            else
            {
                printf("[WARN] 编码第 %d 帧失败，继续…\n", totalFrames + 1);
            }
        }

        // === III.e 释放帧资源（必须配对 AcquireNextFrame + ReleaseFrame） ===
        desktopTexture->Release();
        hr = dup->ReleaseFrame();
        if (FAILED(hr))
        {
            printf("[WARN] ReleaseFrame 失败: HRESULT=0x%08lX\n", hr);
        }
        frameResource->Release();

        // === III.f 帧率控制 ===
        //   桌面无变化时 AcquireNextFrame 会超时，天然起到节流作用。
        //   桌面变化剧烈时需主动限帧。此处用简单策略：每捕获一帧就 Sleep
        //   到下一帧的时间点。
        //   生产代码应考虑使用高精度定时器（QTimer / std::chrono）和
        //   AcquireNextFrame 的非阻塞模式 (timeout=0)。
        DWORD elapsedSinceStart = GetTickCount() - startTick;
        DWORD targetElapsed     = (DWORD)(totalFrames * 1000 / RECORD_FPS);
        if (elapsedSinceStart < targetElapsed && targetElapsed - elapsedSinceStart > 1)
        {
            Sleep(targetElapsed - elapsedSinceStart);
        }
    }

    printf("\n[INFO] 录制循环结束: 捕获尝试 %d 次, 成功编码 %d 帧 (%d 秒)\n",
           capAttempts, totalFrames, RECORD_DURATION_SEC);

    // ========================================================================
    // 阶段 IV: 冲刷编码器 & 释放所有资源
    // ========================================================================

    // 冲刷编码器（获取内部缓冲的延迟帧）
    flushEncoder(encCtx);

    // 释放 FFmpeg 资源（内部调用 av_write_trailer 写入文件尾）
    closeEncoder(encCtx);

    // 释放 CPU 帧缓冲
    delete[] frameBuffer;

    // 释放 DXGI / D3D11 资源（逆序释放）
    stagingTexture->Release();
    dup->Release();
    d3dContext->Release();
    d3dDevice->Release();

    printf("\n=== 录制测试完成: %s (%d 帧) ===\n",
           OUTPUT_FILENAME, totalFrames);

    return 0;
}