#include "../hpp/screenrecorder.hpp"
#include <QDebug>
#include <QDateTime>

/**
 * @brief ScreenRecorder构造函数
 * @param parent 父对象指针
 */
ScreenRecorder::ScreenRecorder(QObject *parent)
    : QObject(parent)
    , m_recordState(Stopped)
    , m_recordThread(nullptr)
    , m_recordingTimer()
    , m_recordingSeconds(0)
    , m_currentFileName("")
    , m_avFormatContext(nullptr)
    , m_avCodecContext(nullptr)
    , m_avFilterGraph(nullptr)
{
    qDebug() << "ScreenRecorder初始化完成";
}

/**
 * @brief ScreenRecorder析构函数
 */
ScreenRecorder::~ScreenRecorder()
{
    stopRecording();
    cleanup();
    qDebug() << "ScreenRecorder析构完成";
}

/**
 * @brief 初始化录制环境
 * @return 初始化是否成功
 */
bool ScreenRecorder::initialize()
{
    qDebug() << "初始化录制环境（功能待实现）";

    // 第二阶段实现FFmpeg环境初始化
    return true;
}

/**
 * @brief 开始录制
 * @return 开始录制是否成功
 */
bool ScreenRecorder::startRecording()
{
    qDebug() << "开始录制（功能待实现）";

    if (m_recordState.load() != Stopped) {
        qWarning() << "录制状态不正确，无法开始录制";
        return false;
    }

    // 生成文件名
    m_currentFileName = generateFileName();

    // 设置录制状态
    setRecordState(Recording);
    m_recordingSeconds = 0;

    // 启动计时器
    m_recordingTimer.start();

    // 使用QThread::create()创建录制线程
    // 这样recordingLoop()将在子线程中执行
    m_recordThread = QThread::create([this]() {
        recordingLoop();
    });

    // 线程结束后自动删除
    connect(m_recordThread, &QThread::finished, m_recordThread, &QThread::deleteLater);

    // 启动录制线程
    m_recordThread->start();

    // 发射录制时间更新信号
    emit recordingTimeUpdated(m_recordingSeconds);

    return true;
}

/**
 * @brief 停止录制
 */
void ScreenRecorder::stopRecording()
{
    qDebug() << "停止录制（功能待实现）";

    if (m_recordState.load() != Recording) {
        return;
    }

    // 设置停止状态（原子操作，子线程会检测到）
    setRecordState(Stopped);

    // 等待录制线程结束
    if (m_recordThread && m_recordThread->isRunning()) {
        m_recordThread->quit();
        m_recordThread->wait();
    }

    qDebug() << "录制停止，文件:" << m_currentFileName;
}

/**
 * @brief 获取当前录制状态
 * @return 录制状态
 */
ScreenRecorder::RecordState ScreenRecorder::getRecordState() const
{
    return static_cast<RecordState>(m_recordState.load());
}

/**
 * @brief 获取已录制时间（秒）
 * @return 录制时间
 */
int ScreenRecorder::getRecordingTime() const
{
    return m_recordingSeconds;
}

/**
 * @brief 获取当前录制文件名
 * @return 文件名
 */
QString ScreenRecorder::getCurrentFileName() const
{
    return m_currentFileName;
}

/**
 * @brief 录制线程主循环（在子线程中执行）
 *
 * 使用QElapsedTimer精确计时，每50ms处理一帧（20fps），
 * 每秒发射一次录制时间更新信号。
 */
void ScreenRecorder::recordingLoop()
{
    qDebug() << "录制线程开始（功能待实现）";

    // 第二阶段实现具体的录制逻辑
    // 这里先模拟录制过程

    int lastEmittedSecond = 0;

    while (m_recordState.load() == Recording) {
        // 模拟录制一帧（20fps = 50ms/帧）
        QThread::msleep(50);

        // 使用QElapsedTimer获取精确的录制时间
        qint64 elapsedMs = m_recordingTimer.elapsed();
        int currentSecond = static_cast<int>(elapsedMs / 1000);

        // 每秒发射一次时间更新信号
        if (currentSecond > lastEmittedSecond) {
            lastEmittedSecond = currentSecond;
            m_recordingSeconds = currentSecond;
            emit recordingTimeUpdated(currentSecond);

            // 每5秒输出一次日志
            if (currentSecond % 5 == 0) {
                qDebug() << "录制中..." << currentSecond << "秒";
            }
        }
    }

    qDebug() << "录制线程结束";
}

/**
 * @brief 清理资源
 */
void ScreenRecorder::cleanup()
{
    qDebug() << "清理录制资源（功能待实现）";

    // 第二阶段实现FFmpeg资源清理

    // 清理线程
    if (m_recordThread) {
        if (m_recordThread->isRunning()) {
            m_recordThread->quit();
            m_recordThread->wait();
        }
        // 注意：线程设置了deleteLater，不需要手动delete
        m_recordThread = nullptr;
    }
}

/**
 * @brief 设置录制状态（线程安全）
 * @param state 新的录制状态
 */
void ScreenRecorder::setRecordState(RecordState state)
{
    int oldState = m_recordState.exchange(static_cast<int>(state));

    if (oldState != static_cast<int>(state)) {
        qDebug() << "录制状态改变:" << oldState << "->" << state;

        // 发射状态改变信号（信号在主线程中发射）
        emit recordStateChanged(state);
    }
}

/**
 * @brief 生成文件名
 * @return 生成的文件名
 */
QString ScreenRecorder::generateFileName() const
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString fileName = currentTime.toString("MMdd-hh-mm-ss-zzz");

    fileName += ".mp4";

    qDebug() << "生成文件名:" << fileName;

    return fileName;
}
