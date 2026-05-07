#ifndef SCREENRECORDER_HPP
#define SCREENRECORDER_HPP

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <atomic>

class QThread;
struct AVFormatContext;
struct AVCodecContext;
struct AVFilterGraph;
struct AVFilterContext;
struct AVFrame;
struct AVPacket;
struct AVStream;

class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    enum RecordState
    {
        Stopped,
        Recording,
        Error
    };
    Q_ENUM(RecordState)

    explicit ScreenRecorder(QObject *parent = nullptr);
    ~ScreenRecorder();

    bool initialize();
    void setOutputDir(const QString &dir);
    bool startRecording();
    void stopRecording();

    RecordState getRecordState() const;
    int getRecordingTime() const;
    QString getCurrentFileName() const;

signals:
    void recordStateChanged(int state);
    void recordingTimeUpdated(int seconds);
    void recordError(const QString &errorMessage);

private:
    bool openInput();
    bool createOutput();
    bool createFilterGraph();
    void recordingLoop();
    void closeInput();
    void closeOutput();
    void cleanup();
    void setRecordState(RecordState state);

    std::atomic<int> m_recordState;
    QThread *m_recordThread;
    QElapsedTimer m_recordingTimer;
    std::atomic<int> m_recordingSeconds;
    QString m_currentFileName;
    QString m_outputDir;

    AVFormatContext *m_inputFmtCtx;
    AVFormatContext *m_outputFmtCtx;
    AVCodecContext *m_encoderCtx;
    AVStream *m_outputStream;
    AVFilterGraph *m_filterGraph;
    AVFilterContext *m_bufferSrcCtx;
    AVFilterContext *m_bufferSinkCtx;
    AVFrame *m_frame;
    AVPacket *m_packet;
    int m_frameCount;
};

#endif // SCREENRECORDER_HPP
