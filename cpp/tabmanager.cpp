#include "../hpp/tabmanager.hpp"
#include <QTabWidget>
#include <QDebug>

/**
 * @brief TabManager构造函数
 * @param tabWidget 标签页控件指针
 * @param parent 父对象指针
 */
TabManager::TabManager(QTabWidget *tabWidget, QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_currentState(Idle)
    , m_isPlaybackPaused(false)
{
    // 连接标签页切换信号
    if (m_tabWidget) {
        connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
            if (index == 0) {
                switchToRecordingTab();
            } else if (index == 1) {
                switchToPlaybackTab();
            }
        });
    }
    
    qDebug() << "TabManager初始化完成";
}

/**
 * @brief TabManager析构函数
 */
TabManager::~TabManager()
{
    qDebug() << "TabManager析构完成";
}

/**
 * @brief 获取当前应用程序状态
 * @return 当前状态
 */
TabManager::AppState TabManager::currentState() const
{
    return m_currentState;
}

/**
 * @brief 切换到录制标签页
 */
void TabManager::switchToRecordingTab()
{
    qDebug() << "切换到录制标签页";
    
    // 根据当前状态更新界面
    updateUIState();
}

/**
 * @brief 切换到播放标签页
 */
void TabManager::switchToPlaybackTab()
{
    qDebug() << "切换到播放标签页";
    
    // 根据当前状态更新界面
    updateUIState();
}

/**
 * @brief 开始录制任务
 * @return 是否成功开始录制
 */
bool TabManager::startRecording()
{
    qDebug() << "开始录制任务（功能待实现）";
    
    if (!canStartRecording()) {
        qWarning() << "无法开始录制，当前状态:" << m_currentState;
        return false;
    }
    
    // 设置录制状态
    setState(Recording);
    
    // 发射录制开始信号
    emit recordingStarted();
    
    return true;
}

/**
 * @brief 停止录制任务
 */
void TabManager::stopRecording()
{
    qDebug() << "停止录制任务（功能待实现）";
    
    if (m_currentState != Recording) {
        qWarning() << "当前不在录制状态，无法停止录制";
        return;
    }
    
    // 设置空闲状态
    setState(Idle);
    
    // 发射录制停止信号
    emit recordingStopped();
}

/**
 * @brief 开始播放任务
 * @return 是否成功开始播放
 */
bool TabManager::startPlayback()
{
    qDebug() << "开始播放任务（功能待实现）";
    
    if (!canStartPlayback()) {
        qWarning() << "无法开始播放，当前状态:" << m_currentState;
        return false;
    }
    
    // 设置播放状态
    setState(Playing);
    m_isPlaybackPaused = false;
    
    // 发射播放开始信号
    emit playbackStarted();
    
    return true;
}

/**
 * @brief 停止播放任务
 */
void TabManager::stopPlayback()
{
    qDebug() << "停止播放任务（功能待实现）";
    
    if (m_currentState != Playing) {
        qWarning() << "当前不在播放状态，无法停止播放";
        return;
    }
    
    // 设置空闲状态
    setState(Idle);
    m_isPlaybackPaused = false;
    
    // 发射播放停止信号
    emit playbackStopped();
}

/**
 * @brief 暂停播放任务
 */
void TabManager::pausePlayback()
{
    qDebug() << "暂停播放任务（功能待实现）";
    
    if (m_currentState != Playing || m_isPlaybackPaused) {
        qWarning() << "当前不在播放状态或已暂停，无法暂停播放";
        return;
    }
    
    m_isPlaybackPaused = true;
    
    // 发射播放暂停信号
    emit playbackPaused();
}

/**
 * @brief 恢复播放任务
 */
void TabManager::resumePlayback()
{
    qDebug() << "恢复播放任务（功能待实现）";
    
    if (m_currentState != Playing || !m_isPlaybackPaused) {
        qWarning() << "当前不在播放状态或未暂停，无法恢复播放";
        return;
    }
    
    m_isPlaybackPaused = false;
    
    // 发射播放恢复信号
    emit playbackResumed();
}

/**
 * @brief 设置应用程序状态
 * @param newState 新的状态
 */
void TabManager::setState(AppState newState)
{
    if (m_currentState != newState) {
        AppState oldState = m_currentState;
        m_currentState = newState;
        
        qDebug() << "应用程序状态改变:" << oldState << "->" << newState;
        
        // 发射状态改变信号
        emit stateChanged(newState);
        
        // 更新界面状态
        updateUIState();
    }
}

/**
 * @brief 检查是否可以开始录制
 * @return 是否可以开始录制
 */
bool TabManager::canStartRecording() const
{
    // 只有在空闲状态才能开始录制
    return m_currentState == Idle;
}

/**
 * @brief 检查是否可以开始播放
 * @return 是否可以开始播放
 */
bool TabManager::canStartPlayback() const
{
    // 只有在空闲状态才能开始播放
    return m_currentState == Idle;
}

/**
 * @brief 更新界面状态
 */
void TabManager::updateUIState()
{
    if (!m_tabWidget) {
        return;
    }
    
    // 根据当前状态更新标签页的可用性
    // 这里只是框架，具体实现将在第二阶段和第三阶段完成
    
    qDebug() << "更新界面状态，当前状态:" << m_currentState;
}