// SPDX-License-Identifier: MPL-2.0

#include "TelegramPlugin.h"
#include "core/SendHttpHelper.h"

#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>

namespace { QLineEdit* field(QWidget* w, const char* n) { return w->findChild<QLineEdit*>(n); } }

QVariantMap TelegramPlugin::metadata() const { return {{"name", name()}, {"version", version()}, {"description", description()}, {"author", "Absand Team"}, {"channel", channelName()}}; }
QVariantMap TelegramPlugin::defaultConfiguration() const { return {{"botToken", ""}, {"chatId", ""}, {"messageTemplate", tr("Archive password: {password}")}}; }

QWidget* TelegramPlugin::createConfigWidget(QWidget* parent)
{
    auto* widget = new QWidget(parent); auto* layout = new QFormLayout(widget);
    auto add = [widget, layout](const QString& label, const char* name, bool secret = false) {
        auto* edit = new QLineEdit(widget); edit->setObjectName(name);
        if (secret) edit->setEchoMode(QLineEdit::Password); layout->addRow(label, edit);
    };
    add(tr("Bot token"), "botToken", true); add(tr("Chat ID"), "chatId"); add(tr("Message template"), "messageTemplate");
    return widget;
}

void TelegramPlugin::loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const
{ const auto d = defaultConfiguration(); for (const char* k : {"botToken", "chatId", "messageTemplate"}) if (auto* e = field(widget, k)) e->setText(config.value(k, d.value(k)).toString()); }
QVariantMap TelegramPlugin::saveConfigurationWidget(QWidget* widget) const
{ QVariantMap c; for (const char* k : {"botToken", "chatId", "messageTemplate"}) if (auto* e = field(widget, k)) c[k] = e->text().trimmed(); return c; }
bool TelegramPlugin::validateConfiguration(const QVariantMap& c, QString* error) const
{
    if (c.value("botToken").toString().trimmed().isEmpty() || c.value("chatId").toString().trimmed().isEmpty()) { if (error) *error = tr("Bot token and chat ID are required."); return false; }
    if (!c.value("messageTemplate").toString().contains("{password}")) { if (error) *error = tr("The message template must contain {password}."); return false; }
    return true;
}
void TelegramPlugin::send(const QString& password, const QVariantMap& c, std::function<void(bool, QString)> cb)
{
    QString error; if (password.isEmpty()) error = tr("Cannot send an empty password."); else if (!validateConfiguration(c, &error)) {} else {
        const QString url = QStringLiteral("https://api.telegram.org/bot%1/sendMessage").arg(c.value("botToken").toString().trimmed());
        const QByteArray body = QJsonDocument(QJsonObject{{"chat_id", c.value("chatId").toString().trimmed()}, {"text", SendHttpHelper::messageFromTemplate(c, password)}}).toJson(QJsonDocument::Compact);
        if (SendHttpHelper::postJson(url, body, {}, &error)) { if (cb) cb(true, tr("Password sent over Telegram.")); return; }
    }
    if (cb) cb(false, error);
}
