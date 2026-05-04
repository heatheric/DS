#include "../hpp/filemanager.hpp"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QCoreApplication>

/**
 * @brief FileManager构造函数
 * @param parent 父对象指针
 */
FileManager::FileManager(QObject *parent)
    : QObject(parent), m_videodirPath(""), m_currentDateDir(""), m_videoFiles()
{
    qDebug() << "FileManager初始化完成";
}

/**
 * @brief FileManager析构函数
 */
FileManager::~FileManager()
{
    qDebug() << "FileManager析构完成";
}

/**
 * @brief 初始化文件管理器
 * @return 初始化是否成功
 */
bool FileManager::initialize()
{
    qDebug() << "初始化文件管理器";

    // 设置workdir路径（使用exe所在目录）
    m_videodirPath = QCoreApplication::applicationDirPath() + "/videodir";

    if (!ensureWorkdirExists())
    {
        qWarning() << "无法创建workdir根目录:" << m_videodirPath;
        return false;
    }

    QString dateFolder = QDateTime::currentDateTime().toString("yyyyMMdd");
    m_currentDateDir = m_videodirPath + "/" + dateFolder;

    QDir dateDir(m_currentDateDir);
    if (!dateDir.exists())
    {
        if (dateDir.mkpath("."))
        {
            qDebug() << "日期目录创建成功:" << m_currentDateDir;
        }
        else
        {
            qWarning() << "日期目录创建失败:" << m_currentDateDir;
            return false;
        }
    }
    else
    {
        qDebug() << "日期目录已存在:" << m_currentDateDir;
    }

    scanVideoFiles();

    qDebug() << "文件管理器初始化完成，workdir路径:" << m_videodirPath;
    qDebug() << "当前日期目录:" << m_currentDateDir;
    qDebug() << "找到" << m_videoFiles.size() << "个视频文件";

    return true;
}

/**
 * @brief 获取workdir目录路径
 * @return 目录路径
 */
QString FileManager::getWorkdirPath() const
{
    return m_videodirPath;
}

/**
 * @brief 获取当前日期子目录路径
 * @return 日期目录路径
 */
QString FileManager::getCurrentDateDir() const
{
    return m_currentDateDir;
}

/**
 * @brief 生成新的视频文件名
 * @return 生成的文件名（不含路径）
 */
QString FileManager::generateVideoFileName() const
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString fileName = currentTime.toString("MMdd-hh-mm-ss-zzz");

    fileName += ".mp4";

    qDebug() << "生成视频文件名:" << fileName;

    return fileName;
}

/**
 * @brief 获取视频文件列表
 * @return 文件名列表
 */
QStringList FileManager::getVideoFileList() const
{
    return m_videoFiles;
}

/**
 * @brief 检查文件是否存在
 * @param fileName 文件名
 * @return 文件是否存在
 */
bool FileManager::fileExists(const QString &fileName) const
{
    QString fullPath = getFullPath(fileName);
    return QFile::exists(fullPath);
}

/**
 * @brief 获取文件完整路径
 * @param fileName 文件名
 * @return 完整文件路径
 */
QString FileManager::getFullPath(const QString &fileName) const
{
    return m_currentDateDir + "/" + fileName;
}

/**
 * @brief 获取文件大小
 * @param fileName 文件名
 * @return 文件大小（字节），-1表示文件不存在
 */
qint64 FileManager::getFileSize(const QString &fileName) const
{
    QString fullPath = getFullPath(fileName);
    QFileInfo fileInfo(fullPath);

    if (fileInfo.exists())
    {
        return fileInfo.size();
    }

    return -1;
}

/**
 * @brief 删除文件
 * @param fileName 文件名
 * @return 删除是否成功
 */
bool FileManager::deleteFile(const QString &fileName)
{
    QString fullPath = getFullPath(fileName);

    if (QFile::exists(fullPath))
    {
        bool success = QFile::remove(fullPath);

        if (success)
        {
            qDebug() << "文件删除成功:" << fileName;

            // 更新文件列表
            scanVideoFiles();

            // 发射文件列表更新信号
            emit fileListUpdated();
        }
        else
        {
            qWarning() << "文件删除失败:" << fileName;
        }

        return success;
    }

    qWarning() << "文件不存在，无法删除:" << fileName;
    return false;
}

/**
 * @brief 确保workdir目录存在
 * @return 目录是否存在或创建成功
 */
bool FileManager::ensureWorkdirExists()
{
    QDir workdir(m_videodirPath);

    if (!workdir.exists())
    {
        qDebug() << "创建workdir目录:" << m_videodirPath;

        if (workdir.mkpath("."))
        {
            qDebug() << "workdir目录创建成功";
            return true;
        }
        else
        {
            qWarning() << "workdir目录创建失败";
            return false;
        }
    }

    qDebug() << "workdir目录已存在:" << m_videodirPath;
    return true;
}

/**
 * @brief 扫描workdir目录下的视频文件
 */
void FileManager::scanVideoFiles()
{
    qDebug() << "扫描视频文件";

    QDir dateDir(m_currentDateDir);

    m_videoFiles.clear();

    QStringList allFiles = dateDir.entryList(QDir::Files);

    // 过滤视频文件
    for (const QString &file : allFiles)
    {
        if (isSupportedFormat(file))
        {
            m_videoFiles.append(file);
        }
    }

    // 按文件名排序（最新的文件在前）
    m_videoFiles.sort();
    std::reverse(m_videoFiles.begin(), m_videoFiles.end());

    qDebug() << "扫描完成，找到" << m_videoFiles.size() << "个视频文件";
}

/**
 * @brief 检查文件格式是否支持
 * @param fileName 文件名
 * @return 是否支持该格式
 */
bool FileManager::isSupportedFormat(const QString &fileName) const
{
    // 支持常见的视频文件格式
    QStringList supportedFormats = {
        ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"};

    for (const QString &format : supportedFormats)
    {
        if (fileName.toLower().endsWith(format))
        {
            return true;
        }
    }

    return false;
}