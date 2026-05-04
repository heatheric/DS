#include "../hpp/mainwindow.hpp"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QCoreApplication>
#include <QTextCodec>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/**
 * @brief 应用程序主入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 */
int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QApplication app(argc, argv);

    // 设置应用程序属性
    app.setApplicationName("屏幕录制与播放软件");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("开发团队");

    // 将工作目录设置为exe文件所在目录（需求规格要求）
    QString exeDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(exeDir);

    qDebug() << "应用程序启动";
    qDebug() << "应用程序名称:" << app.applicationName();
    qDebug() << "应用程序版本:" << app.applicationVersion();
    qDebug() << "工作目录:" << QDir::currentPath();

    try
    {
        // 创建主窗口
        MainWindow mainWindow;

        // 显示主窗口
        mainWindow.show();

        qDebug() << "主窗口显示完成";

        // 进入应用程序主循环
        return app.exec();
    }
    catch (const std::exception &e)
    {
        // 处理异常情况
        qCritical() << "应用程序异常:" << e.what();

        QMessageBox::critical(nullptr, "应用程序错误",
                              QString("应用程序发生严重错误:\n%1\n\n应用程序将退出。").arg(e.what()));

        return -1;
    }
    catch (...)
    {
        // 处理未知异常
        qCritical() << "应用程序发生未知异常";

        QMessageBox::critical(nullptr, "应用程序错误",
                              "应用程序发生未知严重错误。\n\n应用程序将退出。");

        return -1;
    }
}