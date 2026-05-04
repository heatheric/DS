#include "../hpp/videoplayer.hpp"
#include <QDebug>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent), m_playState(Stopped), m_playThread(nullptr),
      m_currentFile(""), m_duration(0), m_currentPosition(0),
      m_videoWidth(0), m_videoHeight(0),
      m_fmtCtx(nullptr), m_codecCtx(nullptr), m_frame(nullptr),
      m_rgbFrame(nullptr), m_swsCtx(nullptr), m_packet(nullptr),
      m_videoStreamIdx(-1)
{
    qDebug() << "VideoPlayer初始化完成";
}

VideoPlayer::~VideoPlayer()
{
    stopPlayback();
    cleanup();
    qDebug() << "VideoPlayer析构完成";
}

bool VideoPlayer::initialize()
{
    qDebug() << "初始化播放环境";
    return true;
}

bool VideoPlayer::openFile(const QString &filePath)
{
    qDebug() << "打开视频文件:" << filePath;

    if (m_playState.load() != Stopped)
    {
        qWarning() << "播放状态不正确，无法打开新文件";
        return false;
    }

    closeVideo();

    int ret = avformat_open_input(&m_fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "avformat_open_input 失败:" << err;
        emit playbackError(QString("无法打开文件: %1").arg(err));
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "avformat_find_stream_info 失败";
        closeVideo();
        return false;
    }

    m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIdx < 0)
    {
        qWarning() << "找不到视频流";
        closeVideo();
        return false;
    }

    AVStream *stream = m_fmtCtx->streams[m_videoStreamIdx];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder)
    {
        qWarning() << "找不到解码器";
        closeVideo();
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(m_codecCtx, stream->codecpar);
    ret = avcodec_open2(m_codecCtx, decoder, nullptr);
    if (ret < 0)
    {
        qWarning() << "avcodec_open2 失败";
        closeVideo();
        return false;
    }

    m_videoWidth = m_codecCtx->width;
    m_videoHeight = m_codecCtx->height;

    if (stream->duration > 0 && stream->time_base.den > 0)
    {
        m_duration = (int)(stream->duration * av_q2d(stream->time_base));
    }
    else if (m_fmtCtx->duration > 0)
    {
        m_duration = (int)(m_fmtCtx->duration / AV_TIME_BASE);
    }
    else
    {
        m_duration = 0;
    }

    qDebug() << "视频信息:" << m_videoWidth << "x" << m_videoHeight
             << "编码:" << avcodec_get_name(m_codecCtx->codec_id)
             << "像素格式:" << av_get_pix_fmt_name(m_codecCtx->pix_fmt)
             << "时长:" << m_duration << "秒";

    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet = av_packet_alloc();

    m_rgbFrame->format = AV_PIX_FMT_BGRA;
    m_rgbFrame->width = m_videoWidth;
    m_rgbFrame->height = m_videoHeight;
    av_frame_get_buffer(m_rgbFrame, 0);

    m_swsCtx = sws_getContext(m_videoWidth, m_videoHeight, m_codecCtx->pix_fmt,
                              m_videoWidth, m_videoHeight, AV_PIX_FMT_BGRA,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx)
    {
        qWarning() << "sws_getContext 失败";
        closeVideo();
        return false;
    }

    m_currentFile = filePath;
    qDebug() << "文件打开成功:" << filePath;
    return true;
}

bool VideoPlayer::startPlayback()
{
    qDebug() << "开始播放";

    if (m_playState.load() != Stopped || m_currentFile.isEmpty())
    {
        qWarning() << "播放状态不正确或未选择文件";
        return false;
    }

    setPlayState(Playing);
    m_currentPosition = 0;
    m_playbackTimer.start();

    m_playThread = QThread::create([this]()
                                   { playbackLoop(); });
    m_playThread->start();

    emit playbackPositionUpdated(0);
    return true;
}

void VideoPlayer::pausePlayback()
{
    qDebug() << "暂停播放";

    if (m_playState.load() != Playing)
    {
        qWarning() << "当前不在播放状态，无法暂停";
        return;
    }

    setPlayState(Paused);
}

void VideoPlayer::resumePlayback()
{
    qDebug() << "恢复播放";

    if (m_playState.load() != Paused)
    {
        qWarning() << "当前不在暂停状态，无法恢复";
        return;
    }

    setPlayState(Playing);
}

void VideoPlayer::stopPlayback()
{
    qDebug() << "停止播放";

    int state = m_playState.load();
    if (state == Stopped)
    {
        return;
    }

    m_playState.store(Stopped);

    if (m_playThread)
    {
        m_playThread->quit();
        m_playThread->wait();
        delete m_playThread;
        m_playThread = nullptr;
    }

    closeVideo();

    emit playStateChanged(Stopped);
    qDebug() << "播放停止";
}

VideoPlayer::PlayState VideoPlayer::getPlayState() const
{
    return static_cast<PlayState>(m_playState.load());
}

int VideoPlayer::getDuration() const
{
    return m_duration;
}

int VideoPlayer::getCurrentPosition() const
{
    return m_currentPosition;
}

int VideoPlayer::getVideoWidth() const
{
    return m_videoWidth;
}

int VideoPlayer::getVideoHeight() const
{
    return m_videoHeight;
}

void VideoPlayer::playbackLoop()
{
    qDebug() << "播放线程开始";

    qint64 pausedElapsed = 0;
    int lastEmittedSecond = 0;

    while (true)
    {
        int state = m_playState.load();
        if (state == Stopped)
            break;

        if (state == Paused)
        {
            qint64 pauseStart = m_playbackTimer.elapsed();
            QThread::msleep(100);
            if (m_playState.load() == Paused)
            {
                pausedElapsed += (m_playbackTimer.elapsed() - pauseStart);
            }
            continue;
        }

        int ret = av_read_frame(m_fmtCtx, m_packet);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                qDebug() << "播放完毕";
                emit playbackFinished();
                break;
            }
            continue;
        }

        if (m_packet->stream_index != m_videoStreamIdx)
        {
            av_packet_unref(m_packet);
            continue;
        }

        ret = avcodec_send_packet(m_codecCtx, m_packet);
        av_packet_unref(m_packet);

        if (ret < 0)
            continue;

        ret = avcodec_receive_frame(m_codecCtx, m_frame);
        if (ret == AVERROR(EAGAIN))
            continue;
        if (ret < 0)
            continue;

        sws_scale(m_swsCtx,
                  m_frame->data, m_frame->linesize, 0, m_videoHeight,
                  m_rgbFrame->data, m_rgbFrame->linesize);

        emit frameReady(m_rgbFrame->data[0], m_videoWidth, m_videoHeight, m_rgbFrame->linesize[0]);

        qint64 effectiveMs = m_playbackTimer.elapsed() - pausedElapsed;
        int currentSecond = (int)(effectiveMs / 1000);

        if (currentSecond > lastEmittedSecond)
        {
            lastEmittedSecond = currentSecond;
            m_currentPosition = currentSecond;
            emit playbackPositionUpdated(currentSecond);
        }

        if (m_duration > 0 && m_currentPosition >= m_duration)
        {
            qDebug() << "播放完成";
            emit playbackFinished();
            break;
        }

        QThread::msleep(50);
    }

    qDebug() << "播放线程结束";
}

void VideoPlayer::closeVideo()
{
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_rgbFrame)
    {
        av_frame_free(&m_rgbFrame);
    }
    if (m_frame)
    {
        av_frame_free(&m_frame);
    }
    if (m_packet)
    {
        av_packet_free(&m_packet);
    }
    if (m_codecCtx)
    {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_fmtCtx)
    {
        avformat_close_input(&m_fmtCtx);
    }

    m_videoStreamIdx = -1;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_duration = 0;
    m_currentFile.clear();
}

void VideoPlayer::cleanup()
{
    qDebug() << "清理播放资源";

    if (m_playThread)
    {
        if (m_playThread->isRunning())
        {
            m_playThread->quit();
            m_playThread->wait();
        }
        delete m_playThread;
        m_playThread = nullptr;
    }
}

void VideoPlayer::setPlayState(PlayState state)
{
    int oldState = m_playState.exchange(static_cast<int>(state));

    if (oldState != static_cast<int>(state))
    {
        qDebug() << "播放状态改变:" << oldState << "->" << state;
        emit playStateChanged(state);
    }
}
