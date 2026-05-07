#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);
    ~FileManager();

    bool initialize();

    QString getWorkdirPath() const;
    QString getCurrentDateDir() const;
    QString generateVideoFileName() const;
    QStringList getVideoFileList() const;

    bool fileExists(const QString &fileName) const;
    QString getFullPath(const QString &fileName) const;
    qint64 getFileSize(const QString &fileName) const;
    bool deleteFile(const QString &fileName);

signals:
    void fileListUpdated();
    void fileError(const QString &errorMessage);

private:
    bool ensureWorkdirExists();
    void scanVideoFiles();
    bool isSupportedFormat(const QString &fileName) const;

    QString m_videodirPath;
    QString m_currentDateDir;
    QStringList m_videoFiles;
};

#endif // FILEMANAGER_HPP
