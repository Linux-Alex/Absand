// SPDX-License-Identifier: MPL-2.0

#include "TeamsPlugin.h"
#include "core/SendHttpHelper.h"
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
QVariantMap TeamsPlugin::metadata()const{return{{"name",name()},{"version",version()},{"description",description()},{"author","Absand Team"},{"channel",channelName()}};}
QVariantMap TeamsPlugin::defaultConfiguration()const{return{{"webhookUrl",""},{"messageTemplate",tr("Archive password: {password}")}};}
QWidget*TeamsPlugin::createConfigWidget(QWidget*p){auto*w=new QWidget(p);auto*l=new QFormLayout(w);auto*u=new QLineEdit(w);u->setObjectName("webhookUrl");u->setEchoMode(QLineEdit::Password);auto*m=new QLineEdit(w);m->setObjectName("messageTemplate");l->addRow(tr("Workflow webhook URL"),u);l->addRow(tr("Message template"),m);return w;}
void TeamsPlugin::loadConfigurationWidget(QWidget*w,const QVariantMap&c)const{auto d=defaultConfiguration();w->findChild<QLineEdit*>("webhookUrl")->setText(c.value("webhookUrl",d.value("webhookUrl")).toString());w->findChild<QLineEdit*>("messageTemplate")->setText(c.value("messageTemplate",d.value("messageTemplate")).toString());}
QVariantMap TeamsPlugin::saveConfigurationWidget(QWidget*w)const{return{{"webhookUrl",w->findChild<QLineEdit*>("webhookUrl")->text().trimmed()},{"messageTemplate",w->findChild<QLineEdit*>("messageTemplate")->text().trimmed()}};}
bool TeamsPlugin::validateConfiguration(const QVariantMap&c,QString*e)const{if(!c.value("webhookUrl").toString().trimmed().startsWith("https://")){if(e)*e=tr("A valid HTTPS Teams workflow webhook URL is required.");return false;}if(!c.value("messageTemplate").toString().contains("{password}")){if(e)*e=tr("The message template must contain {password}.");return false;}return true;}
void TeamsPlugin::send(const QString&p,const QVariantMap&c,std::function<void(bool,QString)>cb){QString e;if(p.isEmpty())e=tr("Cannot send an empty password.");else if(!validateConfiguration(c,&e)){}else{const QString message=SendHttpHelper::messageFromTemplate(c,p);const QJsonObject body{{"type","message"},{"attachments",QJsonArray{QJsonObject{{"contentType","application/vnd.microsoft.card.adaptive"},{"contentUrl",QJsonValue::Null},{"content",QJsonObject{{"$schema","http://adaptivecards.io/schemas/adaptive-card.json"},{"type","AdaptiveCard"},{"version","1.4"},{"body",QJsonArray{QJsonObject{{"type","TextBlock"},{"text",message},{"wrap",true}}}}}}}}}};if(SendHttpHelper::postJson(c.value("webhookUrl").toString().trimmed(),QJsonDocument(body).toJson(QJsonDocument::Compact),{},&e)){if(cb)cb(true,tr("Password sent over Microsoft Teams."));return;}}if(cb)cb(false,e);}
