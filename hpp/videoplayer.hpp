#ifndef VIDEOPLAYER_HPP
#define VIDEOPLAYER_HPP

#include <QWidget>
#include <QElapsedTimer>

class QOpenGLWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QSpinBox;
class QThread;
class FileManager;
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

struct VideoInfo
{
    int width;
    int height;
    double frameRate;
    int duration;
    QString codecName;
    QString pixelFormat;
};

class VideoPlayer : public QWidget
{
    Q_OBJECT

public:
    enum State
    {
        Empty,
        NonEmptyPlaying,
        NonEmptyPaused,
        NonEmptyStopped,
        Error
    };
    Q_ENUM(State)

    explicit VideoPlayer(FileManager *fileManager, QWidget *parent = nullptr);
    ~VideoPlayer();

    State getState() const;
    const VideoInfo &getVideoInfo() const;
    int getFrameCount() const;
    QString getErrorString() const;

signals:
    void stateChanged(VideoPlayer::State state);
    void frameCountUpdated(int frameCount);
    void videoInfoChanged(const VideoInfo &info);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onPlayClicked();
    void onPauseResumeClicked();
    void onStopClicked();
    void onBrowseFolder();
    void onFileSelected(int index);

private:
    void initUI();
    void setState(State state);

    std::atomic<int> m_state;
    VideoInfo m_videoInfo;
    int m_frameCount;
    QString m_errorString;

    FileManager *m_fileManager;

    QOpenGLWidget *m_videoWidget;
    QComboBox *m_fileComboBox;
    QPushButton *m_browseBtn;
    QPushButton *m_playBtn;
    QPushButton *m_pauseResumeBtn;
    QPushButton *m_stopBtn;
    QLabel *m_totalTimeLabel;
    QLabel *m_currentTimeLabel;
    QSpinBox *m_fpsSpinBox;
    QLabel *m_statusLabel;

    QThread *m_playThread;
    QElapsedTimer m_playbackTimer;
    QString m_currentFile;
    int m_totalDuration;

    AVFormatContext *m_fmtCtx;
    AVCodecContext *m_codecCtx;
    AVFrame *m_frame;
    AVFrame *m_rgbFrame;
    SwsContext *m_swsCtx;
    AVPacket *m_packet;
    int m_videoStreamIdx;
};

#endif // VIDEOPLAYER_HPP
