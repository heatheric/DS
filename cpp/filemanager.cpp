#ifndef FILEMANAGER_CPP
#define FILEMANAGER_CPP

#include "../hpp/filemanager.hpp"

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

FileManager::~FileManager()
{
}

bool FileManager::initialize()
{
    return true;
}

QString FileManager::getWorkdirPath() const
{
    return m_videodirPath;
}

QString FileManager::getCurrentDateDir() const
{
    return m_currentDateDir;
}

QString FileManager::generateVideoFileName() const
{
    return QString();
}

QStringList FileManager::getVideoFileList() const
{
    return m_videoFiles;
}

bool FileManager::fileExists(const QString &fileName) const
{
    Q_UNUSED(fileName);
    return false;
}

QString FileManager::getFullPath(const QString &fileName) const
{
    Q_UNUSED(fileName);
    return QString();
}

qint64 FileManager::getFileSize(const QString &fileName) const
{
    Q_UNUSED(fileName);
    return -1;
}

bool FileManager::deleteFile(const QString &fileName)
{
    Q_UNUSED(fileName);
    return false;
}

bool FileManager::ensureWorkdirExists()
{
    return true;
}

void FileManager::scanVideoFiles()
{
}

bool FileManager::isSupportedFormat(const QString &fileName) const
{
    Q_UNUSED(fileName);
    return false;
}

#endif
