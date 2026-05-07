#ifndef RECORDWIDGET_CPP
#define RECORDWIDGET_CPP

#include "../hpp/recordwidget.hpp"
#include "../hpp/screenrecorder.hpp"
#include "../hpp/filemanager.hpp"

RecordWidget::RecordWidget(FileManager *fileManager, QWidget *parent)
    : QWidget(parent)
    , m_fileManager(fileManager)
{
}

RecordWidget::~RecordWidget()
{
}

void RecordWidget::onToggleRecord()
{
}

void RecordWidget::onBrowseFolder()
{
}

void RecordWidget::onRecordStateChanged(int state)
{
    Q_UNUSED(state);
}

void RecordWidget::onRecordingTimeUpdated(int seconds)
{
    Q_UNUSED(seconds);
}

void RecordWidget::onRecordError(const QString &errorMessage)
{
    Q_UNUSED(errorMessage);
}

void RecordWidget::initUI()
{
}

void RecordWidget::connectSignals()
{
}

void RecordWidget::updateButtonText(int state)
{
    Q_UNUSED(state);
}

#endif // RECORDWIDGET_CPP
