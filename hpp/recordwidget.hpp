#ifndef RECORDWIDGET_HPP
#define RECORDWIDGET_HPP

#include <QWidget>

class QPushButton;
class QLabel;
class QLineEdit;
class QTimer;
class ScreenRecorder;
class FileManager;

class RecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecordWidget(FileManager *fileManager, QWidget *parent = nullptr);
    ~RecordWidget();

signals:
    void recordingStarted();
    void recordingStopped();

private slots:
    void onToggleRecord();
    void onBrowseFolder();
    void onRecordStateChanged(int state);
    void onRecordingTimeUpdated(int seconds);
    void onRecordError(const QString &errorMessage);

private:
    void initUI();
    void connectSignals();
    void updateButtonText(int state);

    ScreenRecorder *m_recorder;
    FileManager *m_fileManager;

    QPushButton *m_toggleRecordBtn;
    QLineEdit *m_folderPathEdit;
    QPushButton *m_browseBtn;
    QLabel *m_recordingTimeLabel;
    QLabel *m_currentFileLabel;
    QTimer *m_timer;
};

#endif // RECORDWIDGET_HPP
