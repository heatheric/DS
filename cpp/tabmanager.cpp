#ifndef TABMANAGER_CPP
#define TABMANAGER_CPP

#include "../hpp/tabmanager.hpp"
#include <QTabWidget>

TabManager::TabManager(QTabWidget * /*tabWidget*/, QObject *parent)
    : QObject(parent)
{
}

TabManager::~TabManager()
{
}

TabManager::AppState TabManager::currentState() const
{
    return m_currentState;
}

void TabManager::switchToRecordingTab()
{
}

void TabManager::switchToPlaybackTab()
{
}

bool TabManager::startRecording()
{
    return false;
}

void TabManager::stopRecording()
{
}

bool TabManager::startPlayback()
{
    return false;
}

void TabManager::stopPlayback()
{
}

void TabManager::pausePlayback()
{
}

void TabManager::resumePlayback()
{
}

bool TabManager::canStartRecording() const
{
    return false;
}

bool TabManager::canStartPlayback() const
{
    return false;
}

void TabManager::setState(AppState state)
{
    Q_UNUSED(state);
}

void TabManager::updateUIState()
{
}

#endif // TABMANAGER_CPP
