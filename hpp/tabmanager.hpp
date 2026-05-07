#ifndef TABMANAGER_HPP
#define TABMANAGER_HPP

#include <QObject>

class QTabWidget;

class TabManager : public QObject
{
    Q_OBJECT

public:
    enum AppState
    {
        Idle,
        Recording,
        Playing
    };
    Q_ENUM(AppState)

    explicit TabManager(QTabWidget *tabWidget, QObject *parent = nullptr);
    ~TabManager();

    AppState currentState() const;

public slots:
    void switchToRecordingTab();
    void switchToPlaybackTab();
    bool startRecording();
    void stopRecording();
    bool startPlayback();
    void stopPlayback();
    void pausePlayback();
    void resumePlayback();

signals:
    void stateChanged(TabManager::AppState newState);
    void recordingStarted();
    void recordingStopped();
    void playbackStarted();
    void playbackStopped();
    void playbackPaused();

private:
    bool canStartRecording() const;
    bool canStartPlayback() const;
    void setState(AppState state);
    void updateUIState();

    QTabWidget *m_tabWidget;
    AppState m_currentState;
    bool m_isPlaybackPaused;
    bool m_blockingTabSwitch;
};

#endif // TABMANAGER_HPP
