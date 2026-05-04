#ifndef SCREENRECORDER_HPP
#define SCREENRECORDER_HPP

#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>

/**
 * @brief 屏幕录制类，负责屏幕捕获、视频编码和文件封装
 *
 * 该类使用FFmpeg8 API实现屏幕录制功能，包括：
 * - 使用AVFilterContext、AVFilter等获取屏幕BGRA数据
 * - 无损压缩并立即封装
 * - 固定录制800x600区域，20fps帧率
 *
 * 线程模型：使用QThread::create()创建录制线程，
 * 录制循环在子线程中执行，通过信号槽与主线程通信。
 */
class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 录制状态枚举
     */
    enum RecordState
    {
        Stopped,   ///< 停止状态
        Recording, ///< 录制中
        Error      ///< 错误状态
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit ScreenRecorder(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ScreenRecorder();

    /**
     * @brief 初始化录制环境
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 开始录制
     * @return 开始录制是否成功
     */
    bool startRecording();

    /**
     * @brief 停止录制
     */
    void stopRecording();

    /**
     * @brief 获取当前录制状态
     * @return 录制状态
     */
    RecordState getRecordState() const;

    /**
     * @brief 获取已录制时间（秒）
     * @return 录制时间
     */
    int getRecordingTime() const;

    /**
     * @brief 获取当前录制文件名
     * @return 文件名
     */
    QString getCurrentFileName() const;

signals:
    /**
     * @brief 录制状态改变信号
     * @param state 新的录制状态
     */
    void recordStateChanged(RecordState state);

    /**
     * @brief 录制时间更新信号
     * @param seconds 录制秒数
     */
    void recordingTimeUpdated(int seconds);

    /**
     * @brief 录制错误信号
     * @param errorMessage 错误信息
     */
    void recordError(const QString &errorMessage);

private:
    /**
     * @brief 录制线程主循环（在子线程中执行）
     */
    void recordingLoop();

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 设置录制状态（线程安全）
     * @param state 新的录制状态
     */
    void setRecordState(RecordState state);

    /**
     * @brief 生成文件名
     * @return 生成的文件名
     */
    QString generateFileName() const;

    std::atomic<int> m_recordState; ///< 当前录制状态（原子变量，线程安全）
    QThread *m_recordThread;        ///< 录制线程
    QElapsedTimer m_recordingTimer; ///< 录制计时器（精确计时）
    int m_recordingSeconds;         ///< 录制秒数
    QString m_currentFileName;      ///< 当前文件名

    // FFmpeg相关成员变量（留空，第二阶段实现）
    void *m_avFormatContext; ///< 格式上下文
    void *m_avCodecContext;  ///< 编码器上下文
    void *m_avFilterGraph;   ///< 过滤器图
};

#endif // SCREENRECORDER_HPP
