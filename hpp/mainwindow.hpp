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

class TabManager;
class FileManager;
class ScreenRecorder;
class VideoPlayer;
class VideoRenderWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartRecording();
    void onStopRecording();
    void onStartPlayback();
    void onPausePlayback();
    void onStopPlayback();
    void onFileSelected(int index);
    void updateRecordingTime();

private:
    void initUI();
    void initRecordingTab();
    void initPlaybackTab();
    void connectSignals();
    void updateUIState();
    void refreshFileList();

    QTabWidget *m_tabWidget;
    TabManager *m_tabManager;
    FileManager *m_fileManager;
    ScreenRecorder *m_screenRecorder;
    VideoPlayer *m_videoPlayer;

    QWidget *m_recordingTab;
    QPushButton *m_startRecordBtn;
    QPushButton *m_stopRecordBtn;
    QLabel *m_recordingTimeLabel;
    QLabel *m_currentFileLabel;
    QTimer *m_recordingTimer;
    int m_recordingSeconds;
    QLabel *m_recordSlotLabel;

    QWidget *m_playbackTab;
    QComboBox *m_fileComboBox;
    QPushButton *m_startPlayBtn;
    QPushButton *m_pausePlayBtn;
    QPushButton *m_stopPlayBtn;
    bool m_isPlaybackPaused;
    QLabel *m_playSlotLabel;
    VideoRenderWidget *m_videoWidget;

    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_recordingLayout;
    QVBoxLayout *m_playbackLayout;
};

#endif // MAINWINDOW_HPP
