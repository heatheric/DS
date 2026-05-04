#include "../hpp/mainwindow.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>
#include <QTime>

#include "../hpp/tabmanager.hpp"
#include "../hpp/filemanager.hpp"

/**
 * @brief MainWindow构造函数
 * @param parent 父窗口指针
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_tabWidget(nullptr), m_tabManager(nullptr), m_fileManager(nullptr), m_recordingTab(nullptr), m_startRecordBtn(nullptr), m_stopRecordBtn(nullptr), m_recordingTimeLabel(nullptr), m_currentFileLabel(nullptr), m_recordingTimer(nullptr), m_recordingSeconds(0), m_recordSlotLabel(nullptr), m_playbackTab(nullptr), m_fileComboBox(nullptr), m_startPlayBtn(nullptr), m_pausePlayBtn(nullptr), m_stopPlayBtn(nullptr), m_isPlaybackPaused(false), m_playSlotLabel(nullptr), m_mainLayout(nullptr), m_recordingLayout(nullptr), m_playbackLayout(nullptr)
{
    // 初始化用户界面
    initUI();

    m_fileManager = new FileManager(this);
    if (!m_fileManager->initialize())
    {
        qWarning() << "文件管理器初始化失败";
    }

    connectSignals();

    // 设置窗口属性
    setWindowTitle("屏幕录制与播放软件");
    setMinimumSize(1000, 800);

    qDebug() << QStringLiteral("MainWindow初始化完成");
}

/**
 * @brief MainWindow析构函数
 */
MainWindow::~MainWindow()
{
    // 停止录制计时器
    if (m_recordingTimer && m_recordingTimer->isActive())
    {
        m_recordingTimer->stop();
    }

    // 清理资源
    delete m_recordingTimer;
    delete m_tabManager;

    qDebug() << "MainWindow析构完成";
}

/**
 * @brief 初始化用户界面
 */
void MainWindow::initUI()
{
    // 创建中心窗口部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建主布局
    m_mainLayout = new QVBoxLayout(centralWidget);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    m_mainLayout->addWidget(m_tabWidget);

    // 创建标签页管理器
    m_tabManager = new TabManager(m_tabWidget, this);

    // 初始化录制界面
    initRecordingTab();

    // 初始化播放界面
    initPlaybackTab();

    // 初始化录制计时器
    m_recordingTimer = new QTimer(this);
    m_recordingTimer->setInterval(1000); // 1秒间隔
}

/**
 * @brief 初始化录制界面
 */
void MainWindow::initRecordingTab()
{
    // 创建录制标签页
    m_recordingTab = new QWidget();
    m_recordingLayout = new QVBoxLayout(m_recordingTab);

    // 创建录制控制按钮区域
    QHBoxLayout *recordButtonLayout = new QHBoxLayout();

    // 创建开始录制按钮
    m_startRecordBtn = new QPushButton("开始录制", m_recordingTab);
    m_startRecordBtn->setMinimumHeight(40);
    m_startRecordBtn->setStyleSheet("QPushButton { font-size: 14px; }");

    // 创建停止录制按钮
    m_stopRecordBtn = new QPushButton("停止录制", m_recordingTab);
    m_stopRecordBtn->setMinimumHeight(40);
    m_stopRecordBtn->setStyleSheet("QPushButton { font-size: 14px; }");
    m_stopRecordBtn->setEnabled(false); // 初始禁用

    // 添加按钮到布局
    recordButtonLayout->addWidget(m_startRecordBtn);
    recordButtonLayout->addWidget(m_stopRecordBtn);

    // 创建录制时间显示区域
    QHBoxLayout *timeLayout = new QHBoxLayout();
    QLabel *timeTitleLabel = new QLabel("录制时间:", m_recordingTab);
    m_recordingTimeLabel = new QLabel("00:00", m_recordingTab);
    m_recordingTimeLabel->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; color: #2c3e50; }");

    timeLayout->addWidget(timeTitleLabel);
    timeLayout->addWidget(m_recordingTimeLabel);
    timeLayout->addStretch();

    // 创建当前文件显示区域
    QHBoxLayout *fileLayout = new QHBoxLayout();
    QLabel *fileTitleLabel = new QLabel("当前文件:", m_recordingTab);
    m_currentFileLabel = new QLabel("未开始录制", m_recordingTab);
    m_currentFileLabel->setStyleSheet("QLabel { font-size: 12px; color: #7f8c8d; }");

    fileLayout->addWidget(fileTitleLabel);
    fileLayout->addWidget(m_currentFileLabel);
    fileLayout->addStretch();

    // 添加所有组件到录制布局
    m_recordingLayout->addLayout(recordButtonLayout);
    m_recordingLayout->addLayout(timeLayout);
    m_recordingLayout->addLayout(fileLayout);
    m_recordingLayout->addStretch();

    // 录制界面底部槽响应状态标签
    m_recordSlotLabel = new QLabel("操作: 无 | 槽: 无", m_recordingTab);
    m_recordSlotLabel->setStyleSheet("QLabel { font-size: 11px; color: #95a5a6; padding: 4px; border-top: 1px solid #bdc3c7; }");
    m_recordSlotLabel->setAlignment(Qt::AlignCenter);
    m_recordingLayout->addWidget(m_recordSlotLabel);

    // 添加录制标签页到标签页控件
    m_tabWidget->addTab(m_recordingTab, "录制");
}

/**
 * @brief 初始化播放界面
 */
void MainWindow::initPlaybackTab()
{
    // 创建播放标签页
    m_playbackTab = new QWidget();
    m_playbackLayout = new QVBoxLayout(m_playbackTab);

    // 创建文件选择区域
    QHBoxLayout *fileSelectLayout = new QHBoxLayout();
    QLabel *fileSelectLabel = new QLabel("选择文件:", m_playbackTab);
    m_fileComboBox = new QComboBox(m_playbackTab);
    m_fileComboBox->setMinimumHeight(30);

    // 添加测试文件（第二阶段将替换为真实文件列表）
    m_fileComboBox->addItem("请选择视频文件");
    m_fileComboBox->addItem("测试视频1.mp4");
    m_fileComboBox->addItem("测试视频2.mp4");

    fileSelectLayout->addWidget(fileSelectLabel);
    fileSelectLayout->addWidget(m_fileComboBox);

    // 创建播放控制按钮区域
    QHBoxLayout *playButtonLayout = new QHBoxLayout();

    // 创建开始播放按钮
    m_startPlayBtn = new QPushButton("开始播放", m_playbackTab);
    m_startPlayBtn->setMinimumHeight(40);
    m_startPlayBtn->setStyleSheet("QPushButton { font-size: 14px; }");

    // 创建暂停/恢复播放按钮（切换式）
    m_pausePlayBtn = new QPushButton("暂停/恢复", m_playbackTab);
    m_pausePlayBtn->setMinimumHeight(40);
    m_pausePlayBtn->setStyleSheet("QPushButton { font-size: 14px; }");
    m_pausePlayBtn->setEnabled(false); // 初始禁用

    // 创建停止播放按钮
    m_stopPlayBtn = new QPushButton("停止", m_playbackTab);
    m_stopPlayBtn->setMinimumHeight(40);
    m_stopPlayBtn->setStyleSheet("QPushButton { font-size: 14px; }");
    m_stopPlayBtn->setEnabled(false); // 初始禁用

    // 添加按钮到布局
    playButtonLayout->addWidget(m_startPlayBtn);
    playButtonLayout->addWidget(m_pausePlayBtn);
    playButtonLayout->addWidget(m_stopPlayBtn);

    // 添加所有组件到播放布局
    m_playbackLayout->addLayout(fileSelectLayout);
    m_playbackLayout->addLayout(playButtonLayout);
    m_playbackLayout->addStretch();

    // 播放界面底部槽响应状态标签
    m_playSlotLabel = new QLabel("操作: 无 | 槽: 无", m_playbackTab);
    m_playSlotLabel->setStyleSheet("QLabel { font-size: 11px; color: #95a5a6; padding: 4px; border-top: 1px solid #bdc3c7; }");
    m_playSlotLabel->setAlignment(Qt::AlignCenter);
    m_playbackLayout->addWidget(m_playSlotLabel);

    // 添加播放标签页到标签页控件
    m_tabWidget->addTab(m_playbackTab, "播放");
}

/**
 * @brief 连接信号和槽
 */
void MainWindow::connectSignals()
{
    // 连接录制按钮信号
    connect(m_startRecordBtn, &QPushButton::clicked, this, &MainWindow::onStartRecording);
    connect(m_stopRecordBtn, &QPushButton::clicked, this, &MainWindow::onStopRecording);

    // 连接播放按钮信号
    connect(m_startPlayBtn, &QPushButton::clicked, this, &MainWindow::onStartPlayback);
    connect(m_pausePlayBtn, &QPushButton::clicked, this, &MainWindow::onPausePlayback);
    connect(m_stopPlayBtn, &QPushButton::clicked, this, &MainWindow::onStopPlayback);

    // 连接文件选择信号
    connect(m_fileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFileSelected);

    // 连接录制计时器信号
    connect(m_recordingTimer, &QTimer::timeout, this, &MainWindow::updateRecordingTime);

    // 连接标签页管理器信号
    connect(m_tabManager, &TabManager::stateChanged, this, [this](TabManager::AppState /*state*/)
            { updateUIState(); });
}

/**
 * @brief 开始录制按钮点击槽函数（留空）
 */
void MainWindow::onStartRecording()
{
    qDebug() << "开始录制按钮点击（功能待实现）";

    m_recordSlotLabel->setText(QString("操作: 点击开始录制 | 槽: onStartRecording [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));

    // 第二阶段实现具体录制逻辑
    // 这里先模拟开始录制
    m_recordingSeconds = 0;
    m_recordingTimeLabel->setText("00:00");
    m_currentFileLabel->setText("正在生成文件名...");
    m_recordingTimer->start();

    // 更新按钮状态
    m_startRecordBtn->setEnabled(false);
    m_stopRecordBtn->setEnabled(true);
}

/**
 * @brief 停止录制按钮点击槽函数（留空）
 */
void MainWindow::onStopRecording()
{
    qDebug() << "停止录制按钮点击（功能待实现）";

    m_recordSlotLabel->setText(QString("操作: 点击停止录制 | 槽: onStopRecording [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));

    // 第二阶段实现具体停止逻辑
    // 这里先模拟停止录制
    m_recordingTimer->stop();
    m_currentFileLabel->setText("录制已完成");

    // 更新按钮状态
    m_startRecordBtn->setEnabled(true);
    m_stopRecordBtn->setEnabled(false);
}

/**
 * @brief 开始播放按钮点击槽函数（留空）
 */
void MainWindow::onStartPlayback()
{
    QString selectedFile = m_fileComboBox->currentText();
    qDebug() << "开始播放按钮点击，选择文件:" << selectedFile << "（功能待实现）";

    m_playSlotLabel->setText(QString("操作: 点击开始播放 | 槽: onStartPlayback [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));

    // 第三阶段实现具体播放逻辑
    // 这里先模拟开始播放
    m_startPlayBtn->setEnabled(false);
    m_pausePlayBtn->setEnabled(true);
    m_stopPlayBtn->setEnabled(true);
}

/**
 * @brief 暂停/恢复播放按钮点击槽函数（切换式）
 *
 * 需求规格要求"暂停与恢复"为一个按钮，
 * 点击时在暂停和恢复之间切换。
 */
void MainWindow::onPausePlayback()
{
    qDebug() << "暂停/恢复播放按钮点击（功能待实现）";

    // 第三阶段实现具体暂停/恢复逻辑
    // 这里先模拟暂停/恢复切换
    if (m_isPlaybackPaused)
    {
        m_isPlaybackPaused = false;
        m_pausePlayBtn->setText("暂停/恢复");
        m_playSlotLabel->setText(QString("操作: 点击暂停/恢复(恢复) | 槽: onPausePlayback [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));
        qDebug() << "切换为恢复播放";
    }
    else
    {
        m_isPlaybackPaused = true;
        m_pausePlayBtn->setText("暂停/恢复");
        m_playSlotLabel->setText(QString("操作: 点击暂停/恢复(暂停) | 槽: onPausePlayback [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));
        qDebug() << "切换为暂停播放";
    }
}

/**
 * @brief 停止播放按钮点击槽函数（留空）
 */
void MainWindow::onStopPlayback()
{
    qDebug() << "停止播放按钮点击（功能待实现）";

    m_playSlotLabel->setText(QString("操作: 点击停止播放 | 槽: onStopPlayback [%1]").arg(QTime::currentTime().toString("hh:mm:ss")));

    // 第三阶段实现具体停止逻辑
    // 这里先模拟停止播放
    m_startPlayBtn->setEnabled(true);
    m_pausePlayBtn->setEnabled(false);
    m_stopPlayBtn->setEnabled(false);
    m_isPlaybackPaused = false;
    m_pausePlayBtn->setText("暂停/恢复");
}

/**
 * @brief 文件选择下拉列表改变槽函数（留空）
 */
void MainWindow::onFileSelected(int index)
{
    if (index > 0)
    {
        QString selectedFile = m_fileComboBox->itemText(index);
        qDebug() << "文件选择改变:" << selectedFile << "（功能待实现）";

        m_playSlotLabel->setText(QString("操作: 选择文件(%1) | 槽: onFileSelected [%2]").arg(selectedFile, QTime::currentTime().toString("hh:mm:ss")));

        // 第三阶段实现文件选择逻辑
        m_startPlayBtn->setEnabled(true);
    }
    else
    {
        m_startPlayBtn->setEnabled(false);
    }
}

/**
 * @brief 更新录制时间显示
 */
void MainWindow::updateRecordingTime()
{
    m_recordingSeconds++;
    int minutes = m_recordingSeconds / 60;
    int seconds = m_recordingSeconds % 60;

    QString timeText = QString("%1:%2")
                           .arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(seconds, 2, 10, QLatin1Char('0'));

    m_recordingTimeLabel->setText(timeText);

    // 每5秒更新一次文件名显示（模拟）
    if (m_recordingSeconds % 5 == 0)
    {
        m_currentFileLabel->setText(QString("录制中... 已录制 %1 秒").arg(m_recordingSeconds));
    }
}

/**
 * @brief 更新界面状态
 */
void MainWindow::updateUIState()
{
    TabManager::AppState currentState = m_tabManager->currentState();

    // 根据当前状态更新界面
    switch (currentState)
    {
    case TabManager::Idle:
        // 空闲状态，所有按钮可用
        m_startRecordBtn->setEnabled(true);
        m_startPlayBtn->setEnabled(m_fileComboBox->currentIndex() > 0);
        break;

    case TabManager::Recording:
        // 录制状态，禁用播放相关控件
        m_startPlayBtn->setEnabled(false);
        m_pausePlayBtn->setEnabled(false);
        m_stopPlayBtn->setEnabled(false);
        break;

    case TabManager::Playing:
        // 播放状态，禁用录制相关控件
        m_startRecordBtn->setEnabled(false);
        m_stopRecordBtn->setEnabled(false);
        break;
    }
}