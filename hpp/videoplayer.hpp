#ifndef VIDEOPLAYER_HPP
#define VIDEOPLAYER_HPP

#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

class VideoPlayer : public QObject
{
    Q_OBJECT

public:
    enum PlayState
    {
        Stopped,
        Playing,
        Paused,
        Error
    };

    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool initialize();
    bool openFile(const QString &filePath);
    bool startPlayback();
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();

    PlayState getPlayState() const;
    int getDuration() const;
    int getCurrentPosition() const;
    int getVideoWidth() const;
    int getVideoHeight() const;

signals:
    void playStateChanged(PlayState state);
    void playbackPositionUpdated(int position);
    void playbackFinished();
    void playbackError(const QString &errorMessage);
    void frameReady(const uint8_t *data, int width, int height, int linesize);

private:
    void playbackLoop();
    void closeVideo();
    void cleanup();
    void setPlayState(PlayState state);

    std::atomic<int> m_playState;
    QThread *m_playThread;
    QElapsedTimer m_playbackTimer;
    QString m_currentFile;
    int m_duration;
    int m_currentPosition;
    int m_videoWidth;
    int m_videoHeight;

    AVFormatContext *m_fmtCtx;
    AVCodecContext *m_codecCtx;
    AVFrame *m_frame;
    AVFrame *m_rgbFrame;
    SwsContext *m_swsCtx;
    AVPacket *m_packet;
    int m_videoStreamIdx;
};

#endif // VIDEOPLAYER_HPP
