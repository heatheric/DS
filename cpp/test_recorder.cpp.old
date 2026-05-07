#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "screenrecorder.hpp"
#include "filemanager.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== 录屏功能测试开始 ===";

    FileManager fileManager;
    if (!fileManager.initialize()) {
        qWarning() << "FileManager初始化失败";
        return 1;
    }

    ScreenRecorder recorder;
    recorder.initialize();
    recorder.setOutputDir(fileManager.getCurrentDateDir());

    qDebug() << "输出目录:" << fileManager.getCurrentDateDir();

    QObject::connect(&recorder, &ScreenRecorder::recordStateChanged,
        [](ScreenRecorder::RecordState state) {
            qDebug() << "状态变化:" << static_cast<int>(state);
        });

    QObject::connect(&recorder, &ScreenRecorder::recordingTimeUpdated,
        [](int seconds) {
            qDebug() << "录制时间:" << seconds << "秒";
        });

    QObject::connect(&recorder, &ScreenRecorder::recordError,
        [](const QString &msg) {
            qWarning() << "录制错误:" << msg;
        });

    qDebug() << "尝试开始录制...";
    if (!recorder.startRecording()) {
        qWarning() << "开始录制失败";
        return 1;
    }

    qDebug() << "录制已开始，等待5秒...";

    QTimer::singleShot(5000, [&]() {
        qDebug() << "停止录制...";
        recorder.stopRecording();
        qDebug() << "录制已停止，文件:" << recorder.getCurrentFileName();
        qDebug() << "=== 录屏功能测试完成 ===";
        app.quit();
    });

    return app.exec();
}
