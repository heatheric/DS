#ifndef SCREENRECORDER_CPP
#define SCREENRECORDER_CPP

#include "../hpp/screenrecorder.hpp"

ScreenRecorder::ScreenRecorder(QObject *parent)
    : QObject(parent)
{
}

ScreenRecorder::~ScreenRecorder()
{
}

bool ScreenRecorder::initialize()
{
    return true;
}

void ScreenRecorder::setOutputDir(const QString &dir)
{
    Q_UNUSED(dir);
}

bool ScreenRecorder::startRecording()
{
    return false;
}

void ScreenRecorder::stopRecording()
{
}

ScreenRecorder::RecordState ScreenRecorder::getRecordState() const
{
    return Stopped;
}

int ScreenRecorder::getRecordingTime() const
{
    return m_recordingSeconds.load();
}

QString ScreenRecorder::getCurrentFileName() const
{
    return m_currentFileName;
}

bool ScreenRecorder::openInput()
{
    return false;
}

bool ScreenRecorder::createOutput()
{
    return false;
}

bool ScreenRecorder::createFilterGraph()
{
    return false;
}

void ScreenRecorder::recordingLoop()
{
}

void ScreenRecorder::closeInput()
{
}

void ScreenRecorder::closeOutput()
{
}

void ScreenRecorder::cleanup()
{
}

void ScreenRecorder::setRecordState(RecordState state)
{
    Q_UNUSED(state);
}

#endif // SCREENRECORDER_CPP
