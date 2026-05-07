#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

class QTabWidget;
class TabManager;
class FileManager;
class RecordWidget;
class VideoPlayer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void initUI();
    void initRecordingTab();
    void initPlaybackTab();

    QTabWidget *m_tabWidget;
    TabManager *m_tabManager;
    FileManager *m_fileManager;
    RecordWidget *m_recordWidget;
    VideoPlayer *m_videoPlayer;

    QWidget *m_recordingTab;
    QWidget *m_playbackTab;
};

#endif // MAINWINDOW_HPP
