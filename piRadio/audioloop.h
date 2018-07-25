#ifndef AUDIOLOOP_H
#define AUDIOLOOP_H

#include <QProcess>
#include <QThread>
#include <QObject>
#include <QtMultimedia/QAudioInput>
#include <QtMultimedia/QAudioOutput>
#include <QBuffer>
#include <QtMultimedia/QAudioFormat>
#include "audiohelper.h"

class audioloop : public QObject
{
Q_OBJECT

public:
    audioloop( void );
    ~audioloop( void );
    uint8_t getVolume();
    void setVolume( uint8_t volume);
    bool getLoopready();
    bool getActive();
    void setMute( void );
    void setUnmute( void );
    bool toggleMute( void );
    bool getMute( void );
    uint8_t IncreaseVolume( void );
    uint8_t DecreaseVolume( void );
    void Shutdown();

Q_SIGNALS:
    void NewAudioVolume(uint8_t Volume);
    void NewMusteStaus( bool Muted);

private:
    bool active;
    bool Muted;
    uint8_t CurrentVolume;
    uint8_t SavedVolume;
    bool looprunning;
    QProcess *loopprocess = new QProcess();
    void loopbackerror();
    void ThreadFinished( void );
    audiohelper* aw ;
    uint8_t getVolumeNoMtx();
    void setVolumeNoMtx( uint8_t volume);
    void setMuteNoMtx( void );
    void setUnmuteNoMtx( void );




};

#endif
























