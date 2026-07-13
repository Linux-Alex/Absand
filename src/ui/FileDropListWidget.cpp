// SPDX-License-Identifier: MPL-2.0

#include "FileDropListWidget.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

FileDropListWidget::FileDropListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(true);
}

void FileDropListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QListWidget::dragEnterEvent(event);
}

void FileDropListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QListWidget::dragMoveEvent(event);
}

void FileDropListWidget::dropEvent(QDropEvent* event)
{
    QStringList paths;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }

    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
        return;
    }

    QListWidget::dropEvent(event);
}
