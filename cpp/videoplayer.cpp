#ifndef VIDEOPLAYER_CPP
#define VIDEOPLAYER_CPP

#include "../hpp/videoplayer.hpp"
#include "../hpp/filemanager.hpp"

VideoPlayer::VideoPlayer(FileManager *fileManager, QWidget *parent)
    : QWidget(parent)
    , m_fileManager(fileManager)
{
}

VideoPlayer::~VideoPlayer()
{
}

VideoPlayer::State VideoPlayer::getState() const
{
    return Empty;
}

const VideoInfo &VideoPlayer::getVideoInfo() const
{
    return m_videoInfo;
}

int VideoPlayer::getFrameCount() const
{
    return m_frameCount;
}

QString VideoPlayer::getErrorString() const
{
    return m_errorString;
}

void VideoPlayer::onPlayClicked()
{
}

void VideoPlayer::onPauseResumeClicked()
{
}

void VideoPlayer::onStopClicked()
{
}

void VideoPlayer::onBrowseFolder()
{
}

void VideoPlayer::onFileSelected(int index)
{
    Q_UNUSED(index);
}

void VideoPlayer::initUI()
{
}

void VideoPlayer::setState(State state)
{
    Q_UNUSED(state);
}

#endif // VIDEOPLAYER_CPP
