#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>

/**
 * @brief 文件管理类，负责视频文件的存储、检索和管理
 *
 * 该类管理workdir目录下的视频文件，包括：
 * - 自动创建workdir目录
 * - 按照"年月日时分秒毫秒"格式命名文件
 * - 提供文件列表检索功能
 */
class FileManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FileManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FileManager();

    /**
     * @brief 初始化文件管理器
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 获取workdir目录路径
     * @return 目录路径
     */
    QString getWorkdirPath() const;

    /**
     * @brief 获取当前日期子目录路径
     * @return 日期目录路径
     */
    QString getCurrentDateDir() const;

    /**
     * @brief 生成新的视频文件名
     * @return 生成的文件名（不含路径）
     */
    QString generateVideoFileName() const;

    /**
     * @brief 获取视频文件列表
     * @return 文件名列表
     */
    QStringList getVideoFileList() const;

    /**
     * @brief 检查文件是否存在
     * @param fileName 文件名
     * @return 文件是否存在
     */
    bool fileExists(const QString &fileName) const;

    /**
     * @brief 获取文件完整路径
     * @param fileName 文件名
     * @return 完整文件路径
     */
    QString getFullPath(const QString &fileName) const;

    /**
     * @brief 获取文件大小
     * @param fileName 文件名
     * @return 文件大小（字节），-1表示文件不存在
     */
    qint64 getFileSize(const QString &fileName) const;

    /**
     * @brief 删除文件
     * @param fileName 文件名
     * @return 删除是否成功
     */
    bool deleteFile(const QString &fileName);

signals:
    /**
     * @brief 文件列表更新信号
     */
    void fileListUpdated();

    /**
     * @brief 文件错误信号
     * @param errorMessage 错误信息
     */
    void fileError(const QString &errorMessage);

private:
    /**
     * @brief 确保workdir目录存在
     * @return 目录是否存在或创建成功
     */
    bool ensureWorkdirExists();

    /**
     * @brief 扫描workdir目录下的视频文件
     */
    void scanVideoFiles();

    /**
     * @brief 检查文件格式是否支持
     * @param fileName 文件名
     * @return 是否支持该格式
     */
    bool isSupportedFormat(const QString &fileName) const;

    QString m_videodirPath;   ///< 视频文件根目录路径 (exeDir/video)
    QString m_currentDateDir; ///< 当前日期子目录路径 (exeDir/video/yyyymmdd)
    QStringList m_videoFiles; ///< 视频文件列表
};

#endif // FILEMANAGER_HPP