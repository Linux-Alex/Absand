// SPDX-License-Identifier: MPL-2.0

#ifndef FILEDROPLISTWIDGET_H
#define FILEDROPLISTWIDGET_H

#include <QListWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class FileDropListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit FileDropListWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

#endif // FILEDROPLISTWIDGET_H
