#ifndef TABMANAGER_HPP
#define TABMANAGER_HPP

#include <QObject>
#include <QTabWidget>

/**
 * @brief 标签页管理类，负责管理录制和播放标签页的切换和状态控制
 *
 * 该类确保录制和播放功能在同一时间只能有一个处于活动状态，
 * 提供任务互斥机制和界面状态管理。
 */
class TabManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 应用程序状态枚举
     */
    enum AppState
    {
        Idle,      ///< 空闲状态
        Recording, ///< 录制状态
        Playing    ///< 播放状态
    };

    /**
     * @brief 构造函数
     * @param tabWidget 标签页控件指针
     * @param parent 父对象指针
     */
    explicit TabManager(QTabWidget *tabWidget, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TabManager();

    /**
     * @brief 获取当前应用程序状态
     * @return 当前状态
     */
    AppState currentState() const;

public slots:
    /**
     * @brief 切换到录制标签页
     */
    void switchToRecordingTab();

    /**
     * @brief 切换到播放标签页
     */
    void switchToPlaybackTab();

    /**
     * @brief 开始录制任务
     * @return 是否成功开始录制
     */
    bool startRecording();

    /**
     * @brief 停止录制任务
     */
    void stopRecording();

    /**
     * @brief 开始播放任务
     * @return 是否成功开始播放
     */
    bool startPlayback();

    /**
     * @brief 停止播放任务
     */
    void stopPlayback();

    /**
     * @brief 暂停播放任务
     */
    void pausePlayback();

    /**
     * @brief 恢复播放任务
     */
    void resumePlayback();

signals:
    /**
     * @brief 状态改变信号
     * @param newState 新的应用程序状态
     */
    void stateChanged(AppState newState);

    /**
     * @brief 录制开始信号
     */
    void recordingStarted();

    /**
     * @brief 录制停止信号
     */
    void recordingStopped();

    /**
     * @brief 播放开始信号
     */
    void playbackStarted();

    /**
     * @brief 播放停止信号
     */
    void playbackStopped();

    /**
     * @brief 播放暂停信号
     */
    void playbackPaused();

    /**
     * @brief 播放恢复信号
     */
    void playbackResumed();

private:
    /**
     * @brief 设置应用程序状态
     * @param newState 新的状态
     */
    void setState(AppState newState);

    /**
     * @brief 检查是否可以开始录制
     * @return 是否可以开始录制
     */
    bool canStartRecording() const;

    /**
     * @brief 检查是否可以开始播放
     * @return 是否可以开始播放
     */
    bool canStartPlayback() const;

    /**
     * @brief 更新界面状态
     */
    void updateUIState();

    QTabWidget *m_tabWidget; ///< 标签页控件指针
    AppState m_currentState; ///< 当前应用程序状态
    bool m_isPlaybackPaused; ///< 播放是否暂停
};

#endif // TABMANAGER_HPP