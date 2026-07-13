// SPDX-License-Identifier: MPL-2.0

#include "MatrixPlugin.h"
#include "core/SendHttpHelper.h"
#include <QDateTime>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QUrl>
namespace { QLineEdit* f(QWidget*w,const char*n){return w->findChild<QLineEdit*>(n);} }
QVariantMap MatrixPlugin::metadata()const{return{{"name",name()},{"version",version()},{"description",description()},{"author","Absand Team"},{"channel",channelName()}};}
QVariantMap MatrixPlugin::defaultConfiguration()const{return{{"homeserver","https://matrix.org"},{"accessToken",""},{"roomId",""},{"messageTemplate",tr("Archive password: {password}")}};}
QWidget* MatrixPlugin::createConfigWidget(QWidget*p){auto*w=new QWidget(p);auto*l=new QFormLayout(w);auto add=[w,l](const QString&s,const char*n,bool secret=false){auto*e=new QLineEdit(w);e->setObjectName(n);if(secret)e->setEchoMode(QLineEdit::Password);l->addRow(s,e);};add(tr("Homeserver URL"),"homeserver");add(tr("Access token"),"accessToken",true);add(tr("Room ID"),"roomId");add(tr("Message template"),"messageTemplate");return w;}
void MatrixPlugin::loadConfigurationWidget(QWidget*w,const QVariantMap&c)const{auto d=defaultConfiguration();for(const char*k:{"homeserver","accessToken","roomId","messageTemplate"})if(auto*e=f(w,k))e->setText(c.value(k,d.value(k)).toString());}
QVariantMap MatrixPlugin::saveConfigurationWidget(QWidget*w)const{QVariantMap c;for(const char*k:{"homeserver","accessToken","roomId","messageTemplate"})if(auto*e=f(w,k))c[k]=e->text().trimmed();return c;}
bool MatrixPlugin::validateConfiguration(const QVariantMap&c,QString*e)const{for(const char*k:{"homeserver","accessToken","roomId"})if(c.value(k).toString().trimmed().isEmpty()){if(e)*e=tr("Homeserver, access token, and room ID are required.");return false;}if(!c.value("messageTemplate").toString().contains("{password}")){if(e)*e=tr("The message template must contain {password}.");return false;}return true;}
void MatrixPlugin::send(const QString&p,const QVariantMap&c,std::function<void(bool,QString)>cb){QString e;if(p.isEmpty())e=tr("Cannot send an empty password.");else if(!validateConfiguration(c,&e)){}else{QString base=c.value("homeserver").toString().trimmed();while(base.endsWith('/'))base.chop(1);const QString room=QString::fromLatin1(QUrl::toPercentEncoding(c.value("roomId").toString().trimmed()));const QString txn=QString::number(QDateTime::currentMSecsSinceEpoch());const QString url=QStringLiteral("%1/_matrix/client/v3/rooms/%2/send/m.room.message/%3").arg(base,room,txn);const QByteArray body=QJsonDocument(QJsonObject{{"msgtype","m.text"},{"body",SendHttpHelper::messageFromTemplate(c,p)}}).toJson(QJsonDocument::Compact);if(SendHttpHelper::postJson(url,body,{QStringLiteral("Authorization: Bearer %1").arg(c.value("accessToken").toString().trimmed())},&e)){if(cb)cb(true,tr("Password sent over Matrix."));return;}}if(cb)cb(false,e);}
