#ifndef WEBSOCKETCONNECTOR_H
#define WEBSOCKETCONNECTOR_H

#include <QObject>

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class WebSocketConnector : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketConnector(quint16 port, bool debug = false, QObject *parent = nullptr);
    ~WebSocketConnector();

    void SendNewVolume( uint8_t Volume);
    void SendRawRDSData(uint8_t Byte0, uint8_t Byte1, uint8_t Byte2);
    void SendRDSStationName(QString Name);
    void SendRDSText(QString Text);
    void SendFrequency(uint32_t Frequency);
    void SendSignallevel(uint8_t Level);

 Q_SIGNALS:
    void AudioToggelMute();
    void AudioIncrease();
    void AudioDecrease();
    void PowerDown();
    void ChangeChannel( uint32_t NewChannel);
    void FrequencyStepUp();
    void FrequencyStepDown();
    void FrequencySeek(bool,bool);
    void FrequencySet(uint32_t Frequency);


private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();

Q_SIGNALS:
    void closed();



public Q_SLOTS:
    void onNewAudioLevel(uint8_t Level);
    void onNewMuteStatus(bool Muted);
    void onNewFrequency(uint32_t NewFrequency);
    void onNewRDSRawData(uint8_t Block, uint8_t msb , uint8_t lsb);
    void onNewRDSGroup(uint16_t BlockA, uint16_t BlockB , uint16_t BlockC, uint16_t BlockD);

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    bool m_debug;
};

#endif // WEBSOCKETCONNECTOR_H


