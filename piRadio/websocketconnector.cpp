
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "websocketconnector.h"

QT_USE_NAMESPACE

WebSocketConnector::WebSocketConnector(quint16 port, bool debug, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(new QWebSocketServer(QStringLiteral("Echo Server"),
                                               QWebSocketServer::NonSecureMode, this)),
    m_debug(debug)
{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        if (m_debug)
            qDebug() << "Echoserver listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WebSocketConnector::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &WebSocketConnector::closed);
    }
}

WebSocketConnector::~WebSocketConnector()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void WebSocketConnector::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &WebSocketConnector::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketConnector::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WebSocketConnector::socketDisconnected);

    m_clients << pSocket;
}

void WebSocketConnector::processTextMessage(QString message)
{
    QJsonDocument doc;
    QJsonObject jObj;
    QJsonParseError json_error;
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "Message received:" << message;
    if (pClient) {
        /* Process the message */
        doc=QJsonDocument::fromJson(message.toUtf8(),&json_error);
       if(json_error.error == QJsonParseError::NoError){
           jObj = doc.object();
           QJsonValue CMD = jObj.value("COMMAND");
           QString Command = CMD.toString("ERROR");
           if(Command == "VOLUME_UP"){
               emit AudioIncrease();
           }


           if(Command == "VOLUME_DOWN"){
               emit AudioDecrease();
           }

           if(Command == "FREQUENCY_SET"){
               QJsonValue Freq = jObj.value("Freq");
               emit FrequencySet( (uint32_t)(Freq.toInt(89000000)));
           }

           if(Command == "FREQUENCY_INC"){
               emit FrequencyStepUp();
           }

           if(Command == "FREQUENCY_DEC"){
                emit FrequencyStepDown();
           }

           if(Command == "SEEK_UP"){
                emit FrequencySeek(true,true);
           }

           if(Command == "SEEK_DOWN"){
               emit FrequencySeek(false,true);
           }

           if(Command == "MUTE_TOGGLE"){
                emit AudioToggelMute();
           }

           if(Command == "CHANNEL"){
               QJsonValue Channel = jObj.value("Ch");
               emit ChangeChannel((Channel.toInt(0)));
           }
         }


       }


    }


void WebSocketConnector::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "Binary Message received:" << message;
    if (pClient) {
        //pClient->sendBinaryMessage(message);
    }
}

void WebSocketConnector::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "socketDisconnected:" << pClient;
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

void WebSocketConnector::SendNewVolume( uint8_t Volume){

    QJsonObject recordObject;
    recordObject.insert("VOLUME", QJsonValue((int)Volume));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}

void WebSocketConnector::SendRDSStationName(QString Name){
    QJsonObject recordObject;
    recordObject.insert("RDS_STATION", QJsonValue(Name));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }

}
void WebSocketConnector::SendRDSText(QString Text){
    QJsonObject recordObject;
    recordObject.insert("RDS_TEXT", QJsonValue(Text));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}
void WebSocketConnector::SendFrequency(uint32_t Frequency){
    QJsonObject recordObject;
    recordObject.insert("FREQUENCY", QJsonValue((int)Frequency));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}
void WebSocketConnector::SendSignallevel(uint8_t Level){
    QJsonObject recordObject;
    recordObject.insert("RSSI", QJsonValue((int)Level));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}


void WebSocketConnector::onNewAudioLevel(uint8_t Level){
    this->SendNewVolume(Level);
}
void WebSocketConnector::onNewMuteStatus(bool Muted){

}
void WebSocketConnector::onNewFrequency(uint32_t NewFrequency){
    this->SendFrequency(NewFrequency);
}

void WebSocketConnector::onNewRDSRawData(uint8_t Block, uint8_t msb , uint8_t lsb){
    QJsonObject recordObject;
    QJsonObject Data;
    Data.insert ("Block" , QJsonValue((int)Block));
    Data.insert ("msb" , QJsonValue((int)msb));
    Data.insert ("lsb" , QJsonValue((int)lsb));


    recordObject.insert("RDS_RAW",QJsonObject(Data));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}

void WebSocketConnector::onNewRDSGroup(uint16_t BlockA, uint16_t BlockB , uint16_t BlockC, uint16_t BlockD){
    QJsonObject recordObject;
    QJsonObject Block;
    Block.insert ("A" , QJsonValue((int)BlockA));
    Block.insert ("B" , QJsonValue((int)BlockB));
    Block.insert ("C" , QJsonValue((int)BlockC));
    Block.insert ("D" , QJsonValue((int)BlockD));

    recordObject.insert("RDS_MESSAGE",QJsonObject(Block));
    QJsonDocument doc(recordObject);
    QString JsonObj =  doc.toJson();

    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(JsonObj);
    }
}
