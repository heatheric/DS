#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QDir>
#include <QMutex>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoRenderWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    VideoRenderWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent), m_texture(0), m_frameWidth(0), m_frameHeight(0),
          m_buffer(nullptr), m_bufferSize(0), m_hasNewFrame(false)
    {
        setMinimumSize(800, 600);
        setFocusPolicy(Qt::StrongFocus);
    }

    ~VideoRenderWidget()
    {
        makeCurrent();
        if (m_texture)
        {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
        doneCurrent();
        delete[] m_buffer;
    }

    void presentFrame(const uint8_t *data, int width, int height, int linesize)
    {
        QMutexLocker locker(&m_mutex);

        int bufSize = height * width * 4;
        if (bufSize > m_bufferSize)
        {
            delete[] m_buffer;
            m_buffer = new uint8_t[bufSize];
            m_bufferSize = bufSize;
        }

        if (linesize == width * 4)
        {
            memcpy(m_buffer, data, bufSize);
        }
        else
        {
            for (int y = 0; y < height; y++)
            {
                memcpy(m_buffer + y * width * 4, data + y * linesize, width * 4);
            }
        }

        m_frameWidth = width;
        m_frameHeight = height;
        m_hasNewFrame = true;

        update();
    }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    void paintGL() override
    {
        if (m_hasNewFrame)
        {
            m_hasNewFrame = false;

            QMutexLocker locker(&m_mutex);

            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_frameWidth, m_frameHeight, 0,
                         GL_BGRA, GL_UNSIGNED_BYTE, m_buffer);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        if (m_frameWidth <= 0 || m_frameHeight <= 0)
            return;

        float widgetAspect = (float)width() / (float)height();
        float frameAspect = (float)m_frameWidth / (float)m_frameHeight;

        float drawW = 1.0f;
        float drawH = 1.0f;
        if (widgetAspect > frameAspect)
        {
            drawW = frameAspect / widgetAspect;
        }
        else
        {
            drawH = widgetAspect / frameAspect;
        }

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-drawW, -drawH);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(drawW, -drawH);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(drawW, drawH);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-drawW, drawH);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

private:
    GLuint m_texture;
    int m_frameWidth;
    int m_frameHeight;
    uint8_t *m_buffer;
    int m_bufferSize;
    bool m_hasNewFrame;
    QMutex m_mutex;
};

struct DecoderContext
{
    AVFormatContext *fmtCtx;
    AVCodecContext *codecCtx;
    AVFrame *frame;
    AVFrame *rgbFrame;
    SwsContext *swsCtx;
    int videoStreamIdx;
    int width;
    int height;
    AVPacket *packet;
};

static bool openVideo(const QString &filePath, DecoderContext &ctx)
{
    memset(&ctx, 0, sizeof(ctx));

    int ret = avformat_open_input(&ctx.fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "avformat_open_input 失败:" << err;
        return false;
    }

    ret = avformat_find_stream_info(ctx.fmtCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "avformat_find_stream_info 失败";
        return false;
    }

    ctx.videoStreamIdx = av_find_best_stream(ctx.fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ctx.videoStreamIdx < 0)
    {
        qWarning() << "找不到视频流";
        return false;
    }

    AVStream *stream = ctx.fmtCtx->streams[ctx.videoStreamIdx];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder)
    {
        qWarning() << "找不到解码器";
        return false;
    }

    ctx.codecCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(ctx.codecCtx, stream->codecpar);
    ret = avcodec_open2(ctx.codecCtx, decoder, nullptr);
    if (ret < 0)
    {
        qWarning() << "avcodec_open2 失败";
        return false;
    }

    ctx.width = ctx.codecCtx->width;
    ctx.height = ctx.codecCtx->height;

    qDebug() << "视频信息:" << ctx.width << "x" << ctx.height
             << "编码:" << avcodec_get_name(ctx.codecCtx->codec_id)
             << "像素格式:" << av_get_pix_fmt_name(ctx.codecCtx->pix_fmt);

    ctx.frame = av_frame_alloc();
    ctx.rgbFrame = av_frame_alloc();
    ctx.packet = av_packet_alloc();

    ctx.rgbFrame->format = AV_PIX_FMT_BGRA;
    ctx.rgbFrame->width = ctx.width;
    ctx.rgbFrame->height = ctx.height;
    av_frame_get_buffer(ctx.rgbFrame, 0);

    ctx.swsCtx = sws_getContext(ctx.width, ctx.height, ctx.codecCtx->pix_fmt,
                                ctx.width, ctx.height, AV_PIX_FMT_BGRA,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!ctx.swsCtx)
    {
        qWarning() << "sws_getContext 失败";
        return false;
    }

    return true;
}

static bool decodeNextFrame(DecoderContext &ctx)
{
    while (true)
    {
        int ret = av_read_frame(ctx.fmtCtx, ctx.packet);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
                return false;
            continue;
        }

        if (ctx.packet->stream_index != ctx.videoStreamIdx)
        {
            av_packet_unref(ctx.packet);
            continue;
        }

        ret = avcodec_send_packet(ctx.codecCtx, ctx.packet);
        av_packet_unref(ctx.packet);

        if (ret < 0)
            continue;

        ret = avcodec_receive_frame(ctx.codecCtx, ctx.frame);
        if (ret == AVERROR(EAGAIN))
            continue;
        if (ret < 0)
            return false;

        sws_scale(ctx.swsCtx,
                  ctx.frame->data, ctx.frame->linesize, 0, ctx.height,
                  ctx.rgbFrame->data, ctx.rgbFrame->linesize);

        return true;
    }
}

static void closeVideo(DecoderContext &ctx)
{
    if (ctx.swsCtx)
    {
        sws_freeContext(ctx.swsCtx);
        ctx.swsCtx = nullptr;
    }
    if (ctx.rgbFrame)
    {
        av_frame_free(&ctx.rgbFrame);
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
        avcodec_free_context(&ctx.codecCtx);
    }
    if (ctx.fmtCtx)
    {
        avformat_close_input(&ctx.fmtCtx);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qDebug() << "=== FFmpeg 解码 + QOpenGL 渲染测试 ===";

    QString videoPath = QCoreApplication::applicationDirPath() + "/videodir/20260505/0505-04-56-52-305.mkv";
    qDebug() << "使用视频文件:" << videoPath;

    DecoderContext ctx;
    if (!openVideo(videoPath, ctx))
    {
        qWarning() << "打开视频失败";
        return 1;
    }

    VideoRenderWidget widget;
    widget.setWindowTitle("FFmpeg + QOpenGL 视频播放测试 - 0505-04-56-52-305.mkv");
    widget.resize(ctx.width, ctx.height);
    widget.show();

    QElapsedTimer frameTimer;
    frameTimer.start();
    int frameCount = 0;
    int totalFrames = 0;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]()
                     {
        if (!decodeNextFrame(ctx))
        {
            qDebug() << "播放完毕，总帧数:" << totalFrames;
            timer.stop();
            return;
        }

        widget.presentFrame(ctx.rgbFrame->data[0], ctx.width, ctx.height, ctx.rgbFrame->linesize[0]);
        totalFrames++;

        frameCount++;
        if (frameTimer.elapsed() >= 1000)
        {
            qDebug() << "FPS:" << frameCount << "总帧数:" << totalFrames;
            frameCount = 0;
            frameTimer.restart();
        } });

    timer.start(500);

    int result = app.exec();

    timer.stop();
    closeVideo(ctx);

    qDebug() << "=== 测试完成 ===";
    return result;
}
