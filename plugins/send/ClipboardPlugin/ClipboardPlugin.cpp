// SPDX-License-Identifier: MPL-2.0

#include "ClipboardPlugin.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>

ClipboardPlugin::ClipboardPlugin(QObject* parent) : QObject(parent) {}

void ClipboardPlugin::send(const QString& password,
                           const QVariantMap& config,
                           std::function<void(bool, QString)> resultCallback)
{
    Q_UNUSED(config);
    if (password.isEmpty()) {
        if (resultCallback) {
            resultCallback(false, "Cannot copy an empty password");
        }
        return;
    }
    QGuiApplication::clipboard()->setText(password);
    qDebug() << "Password copied to clipboard";
    
    if (resultCallback) {
        resultCallback(true, "Password copied to clipboard!");
    }
}
