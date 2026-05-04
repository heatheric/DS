#include "../hpp/videoplayer.hpp"
#include <QDebug>

/**
 * @brief VideoPlayer构造函数
 * @param parent 父对象指针
 */
VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent), m_playState(Stopped), m_playThread(nullptr), m_playbackTimer(), m_currentFile(""), m_duration(0), m_currentPosition(0), m_avFormatContext(nullptr), m_avCodecContext(nullptr), m_avFrame(nullptr)
{
    qDebug() << "VideoPlayer初始化完成";
}

/**
 * @brief VideoPlayer析构函数
 */
VideoPlayer::~VideoPlayer()
{
    stopPlayback();
    cleanup();
    qDebug() << "VideoPlayer析构完成";
}

/**
 * @brief 初始化播放环境
 * @return 初始化是否成功
 */
bool VideoPlayer::initialize()
{
    qDebug() << "初始化播放环境（功能待实现）";

    // 第三阶段实现FFmpeg和OpenGL环境初始化
    return true;
}

/**
 * @brief 打开视频文件
 * @param fileName 文件名
 * @return 打开是否成功
 */
bool VideoPlayer::openFile(const QString &fileName)
{
    qDebug() << "打开视频文件:" << fileName << "（功能待实现）";

    if (m_playState.load() != Stopped)
    {
        qWarning() << "播放状态不正确，无法打开新文件";
        return false;
    }

    m_currentFile = fileName;

    // 第三阶段实现文件打开和解析逻辑
    qDebug() << "文件打开成功:" << fileName;
    return true;
}

/**
 * @brief 开始播放
 * @return 开始播放是否成功
 */
bool VideoPlayer::startPlayback()
{
    qDebug() << "开始播放（功能待实现）";

    if (m_playState.load() != Stopped || m_currentFile.isEmpty())
    {
        qWarning() << "播放状态不正确或未选择文件，无法开始播放";
        return false;
    }

    // 设置播放状态
    setPlayState(Playing);
    m_currentPosition = 0;

    // 启动计时器
    m_playbackTimer.start();

    // 使用QThread::create()创建播放线程
    // 这样playbackLoop()将在子线程中执行
    m_playThread = QThread::create([this]()
                                   { playbackLoop(); });

    // 线程结束后自动删除
    connect(m_playThread, &QThread::finished, m_playThread, &QThread::deleteLater);

    // 启动播放线程
    m_playThread->start();

    // 发射播放位置更新信号
    emit playbackPositionUpdated(m_currentPosition);

    return true;
}

/**
 * @brief 暂停播放
 */
void VideoPlayer::pausePlayback()
{
    qDebug() << "暂停播放（功能待实现）";

    if (m_playState.load() != Playing)
    {
        qWarning() << "当前不在播放状态，无法暂停";
        return;
    }

    setPlayState(Paused);
}

/**
 * @brief 恢复播放
 */
void VideoPlayer::resumePlayback()
{
    qDebug() << "恢复播放（功能待实现）";

    if (m_playState.load() != Paused)
    {
        qWarning() << "当前不在暂停状态，无法恢复";
        return;
    }

    setPlayState(Playing);
}

/**
 * @brief 停止播放
 */
void VideoPlayer::stopPlayback()
{
    qDebug() << "停止播放（功能待实现）";

    if (m_playState.load() == Stopped)
    {
        return;
    }

    // 设置停止状态（原子操作，子线程会检测到）
    setPlayState(Stopped);

    // 等待播放线程结束
    if (m_playThread && m_playThread->isRunning())
    {
        m_playThread->quit();
        m_playThread->wait();
    }

    qDebug() << "播放停止";
}

/**
 * @brief 获取当前播放状态
 * @return 播放状态
 */
VideoPlayer::PlayState VideoPlayer::getPlayState() const
{
    return static_cast<PlayState>(m_playState.load());
}

/**
 * @brief 获取视频时长（秒）
 * @return 视频时长
 */
int VideoPlayer::getDuration() const
{
    return m_duration;
}

/**
 * @brief 获取当前播放位置（秒）
 * @return 播放位置
 */
int VideoPlayer::getCurrentPosition() const
{
    return m_currentPosition;
}

/**
 * @brief 播放线程主循环（在子线程中执行）
 *
 * 使用QElapsedTimer精确计时，每50ms处理一帧（20fps），
 * 每秒发射一次播放位置更新信号。
 * 暂停状态下计时器暂停，恢复后继续计时。
 */
void VideoPlayer::playbackLoop()
{
    qDebug() << "播放线程开始（功能待实现）";

    // 第三阶段实现具体的播放逻辑
    // 这里先模拟播放过程

    // 模拟视频时长（60秒）
    m_duration = 60;

    qint64 pausedElapsed = 0; // 暂停期间累积的时间
    int lastEmittedSecond = 0;

    while (m_playState.load() == Playing || m_playState.load() == Paused)
    {
        if (m_playState.load() == Playing)
        {
            // 模拟解码和渲染一帧（20fps = 50ms/帧）
            QThread::msleep(50);

            // 计算实际播放时间（减去暂停时间）
            qint64 currentElapsedMs = m_playbackTimer.elapsed();
            qint64 effectiveMs = currentElapsedMs - pausedElapsed;
            int currentSecond = static_cast<int>(effectiveMs / 1000);

            // 每秒发射一次位置更新信号
            if (currentSecond > lastEmittedSecond)
            {
                lastEmittedSecond = currentSecond;
                m_currentPosition = currentSecond;
                emit playbackPositionUpdated(currentSecond);

                // 每5秒输出一次日志
                if (currentSecond % 5 == 0)
                {
                    qDebug() << "播放中..." << currentSecond << "秒";
                }
            }

            // 检查是否播放完成
            if (m_currentPosition >= m_duration)
            {
                qDebug() << "播放完成";
                emit playbackFinished();
                setPlayState(Stopped);
                break;
            }
        }
        else if (m_playState.load() == Paused)
        {
            // 暂停状态，记录暂停开始时间
            qint64 pauseStartMs = m_playbackTimer.elapsed();
            QThread::msleep(100);

            // 如果仍在暂停状态，累积暂停时间
            if (m_playState.load() == Paused)
            {
                qint64 pauseEndMs = m_playbackTimer.elapsed();
                pausedElapsed += (pauseEndMs - pauseStartMs);
            }
        }

        // 检查是否应该停止
        if (m_playState.load() == Stopped)
        {
            break;
        }
    }

    qDebug() << "播放线程结束";
}

/**
 * @brief 清理资源
 */
void VideoPlayer::cleanup()
{
    qDebug() << "清理播放资源（功能待实现）";

    // 第三阶段实现FFmpeg资源清理

    // 清理线程
    if (m_playThread)
    {
        if (m_playThread->isRunning())
        {
            m_playThread->quit();
            m_playThread->wait();
        }
        // 注意：线程设置了deleteLater，不需要手动delete
        m_playThread = nullptr;
    }
}

/**
 * @brief 设置播放状态（线程安全）
 * @param state 新的播放状态
 */
void VideoPlayer::setPlayState(PlayState state)
{
    int oldState = m_playState.exchange(static_cast<int>(state));

    if (oldState != static_cast<int>(state))
    {
        qDebug() << "播放状态改变:" << oldState << "->" << state;

        // 发射状态改变信号
        emit playStateChanged(state);
    }
}
