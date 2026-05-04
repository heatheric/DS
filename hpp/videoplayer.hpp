#ifndef VIDEOPLAYER_HPP
#define VIDEOPLAYER_HPP

#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>

/**
 * @brief 视频播放类，负责视频文件解码和OpenGL渲染
 *
 * 该类使用FFmpeg8 API实现视频播放功能，包括：
 * - FFmpeg解码视频文件
 * - Qt OpenGL接口进行GPU数据处理和渲染
 * - 播放控制（开始、暂停、停止）
 *
 * 线程模型：使用QThread::create()创建播放线程，
 * 播放循环在子线程中执行，通过信号槽与主线程通信。
 *
 * 注意：QOpenGLWidget由MainWindow管理，不作为本类成员，
 * 本类通过信号将解码后的帧数据传递给主线程进行渲染。
 */
class VideoPlayer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 播放状态枚举
     */
    enum PlayState
    {
        Stopped, ///< 停止状态
        Playing, ///< 播放中
        Paused,  ///< 暂停状态
        Error    ///< 错误状态
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit VideoPlayer(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~VideoPlayer();

    /**
     * @brief 初始化播放环境
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 打开视频文件
     * @param fileName 文件名
     * @return 打开是否成功
     */
    bool openFile(const QString &fileName);

    /**
     * @brief 开始播放
     * @return 开始播放是否成功
     */
    bool startPlayback();

    /**
     * @brief 暂停播放
     */
    void pausePlayback();

    /**
     * @brief 恢复播放
     */
    void resumePlayback();

    /**
     * @brief 停止播放
     */
    void stopPlayback();

    /**
     * @brief 获取当前播放状态
     * @return 播放状态
     */
    PlayState getPlayState() const;

    /**
     * @brief 获取视频时长（秒）
     * @return 视频时长
     */
    int getDuration() const;

    /**
     * @brief 获取当前播放位置（秒）
     * @return 播放位置
     */
    int getCurrentPosition() const;

signals:
    /**
     * @brief 播放状态改变信号
     * @param state 新的播放状态
     */
    void playStateChanged(PlayState state);

    /**
     * @brief 播放位置更新信号
     * @param position 播放位置（秒）
     */
    void playbackPositionUpdated(int position);

    /**
     * @brief 播放完成信号
     */
    void playbackFinished();

    /**
     * @brief 播放错误信号
     * @param errorMessage 错误信息
     */
    void playbackError(const QString &errorMessage);

private:
    /**
     * @brief 播放线程主循环（在子线程中执行）
     */
    void playbackLoop();

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 设置播放状态（线程安全）
     * @param state 新的播放状态
     */
    void setPlayState(PlayState state);

    std::atomic<int> m_playState;  ///< 当前播放状态（原子变量，线程安全）
    QThread *m_playThread;         ///< 播放线程
    QElapsedTimer m_playbackTimer; ///< 播放计时器（精确计时）
    QString m_currentFile;         ///< 当前播放文件
    int m_duration;                ///< 视频时长（秒）
    int m_currentPosition;         ///< 当前播放位置（秒）

    // FFmpeg相关成员变量（留空，第三阶段实现）
    void *m_avFormatContext; ///< 格式上下文
    void *m_avCodecContext;  ///< 解码器上下文
    void *m_avFrame;         ///< 帧数据
};

#endif // VIDEOPLAYER_HPP
