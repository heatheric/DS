#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTimer>

// 前向声明
class TabManager;
class FileManager;

/**
 * @brief 主窗口类，负责应用程序的主界面
 *
 * 该类继承自QMainWindow，包含录制和播放两个标签页，
 * 管理整个应用程序的用户界面和状态控制。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow();

private slots:
    /**
     * @brief 开始录制按钮点击槽函数（留空）
     */
    void onStartRecording();

    /**
     * @brief 停止录制按钮点击槽函数（留空）
     */
    void onStopRecording();

    /**
     * @brief 开始播放按钮点击槽函数（留空）
     */
    void onStartPlayback();

    /**
     * @brief 暂停播放按钮点击槽函数（留空）
     */
    void onPausePlayback();

    /**
     * @brief 停止播放按钮点击槽函数（留空）
     */
    void onStopPlayback();

    /**
     * @brief 文件选择下拉列表改变槽函数（留空）
     */
    void onFileSelected(int index);

    /**
     * @brief 更新录制时间显示
     */
    void updateRecordingTime();

private:
    /**
     * @brief 初始化用户界面
     */
    void initUI();

    /**
     * @brief 初始化录制界面
     */
    void initRecordingTab();

    /**
     * @brief 初始化播放界面
     */
    void initPlaybackTab();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 更新界面状态
     */
    void updateUIState();

    // 界面组件
    QTabWidget *m_tabWidget;   ///< 标签页控件
    TabManager *m_tabManager;  ///< 标签页管理器
    FileManager *m_fileManager; ///< 文件管理器

    // 录制界面组件
    QWidget *m_recordingTab;       ///< 录制标签页
    QPushButton *m_startRecordBtn; ///< 开始录制按钮
    QPushButton *m_stopRecordBtn;  ///< 停止录制按钮
    QLabel *m_recordingTimeLabel;  ///< 录制时间显示标签
    QLabel *m_currentFileLabel;    ///< 当前文件显示标签
    QTimer *m_recordingTimer;      ///< 录制计时器
    int m_recordingSeconds;        ///< 录制秒数
    QLabel *m_recordSlotLabel;     ///< 录制界面槽响应状态标签

    // 播放界面组件
    QWidget *m_playbackTab;      ///< 播放标签页
    QComboBox *m_fileComboBox;   ///< 文件选择下拉列表
    QPushButton *m_startPlayBtn; ///< 开始播放按钮
    QPushButton *m_pausePlayBtn; ///< 暂停/恢复播放按钮（切换式）
    QPushButton *m_stopPlayBtn;  ///< 停止播放按钮
    bool m_isPlaybackPaused;     ///< 播放是否处于暂停状态
    QLabel *m_playSlotLabel;     ///< 播放界面槽响应状态标签

    // 布局
    QVBoxLayout *m_mainLayout;      ///< 主布局
    QVBoxLayout *m_recordingLayout; ///< 录制界面布局
    QVBoxLayout *m_playbackLayout;  ///< 播放界面布局
};

#endif // MAINWINDOW_HPP