#ifndef AUDIOHELPER_H
#define AUDIOHELPER_H


#include <alsa/asoundlib.h>
#include <QObject>
#include "QFuture"
#include <QtConcurrent/QtConcurrent>



class audiohelper : public QThread
{
    /* This is the rds decoder, currently only decoding the stationname and text */

public:
    bool GetActive( void );
    void Shutdown ( void );

signals:
    /* If we can add auto streaming here */

public slots:


private:
void run();
int open_stream(snd_pcm_t **handle, const char *name, snd_pcm_stream_t dir);

bool active = false;
};

#endif // AUDIOHELPER_H
