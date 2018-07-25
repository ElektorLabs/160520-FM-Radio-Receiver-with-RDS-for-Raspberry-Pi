#include <stdio.h>
#include <stdlib.h>
#include <QProcess>
#include <QString>
#include <QThread>
#include <QCoreApplication>
#include <alsa/asoundlib.h>
#include "audioloop.h"
#include "audiohelper.h"


QFuture<void> VolumeWorker;
snd_mixer_elem_t* elem=0;
snd_mixer_t *handle=0;
snd_mixer_selem_id_t *sid=0;
long int min=0;
long int max=0;

QMutex audio_mutex;

audioloop::audioloop(  ){



    const char *card = "default";
    const char *selem_name = "Master";

    /* We will just start alsaloop with the suiting parameter */
    printf("Audioloop Created");
    this->active = true;
    this->looprunning=false;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    elem = snd_mixer_find_selem(handle, sid);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);


    aw = new audiohelper();
    connect(aw, (&audiohelper::finished), this, &audioloop::ThreadFinished);
    aw->start(QThread::HighestPriority);
    this->Muted = false;
    this->setVolume(0);



}

void audioloop::ThreadFinished( void ){
    if(   this->looprunning== true ){
        /* if the thread ends this is a mayor error */

    } else {
        /* Everything is fine */
    }
}

void audioloop::Shutdown(){
   uint8_t killcnt=0;
   aw->Shutdown();
   while(aw->isRunning()==true){
       QThread::msleep(500);
       if(killcnt>5){
           aw->terminate();
       }
   }
}

audioloop::~audioloop(  ){
 snd_mixer_close(handle);
 this->active = false;
 loopprocess->kill();
}


bool audioloop::getLoopready(){

    if(aw == nullptr){
        return false;
    } else {
        if(aw->isFinished()==true){
             aw->start(QThread::HighestPriority);
        }
        return aw->GetActive();
    }
}

bool audioloop::getActive(){
    if(aw == nullptr){
        return false;
    } else {
        return aw->GetActive();
    }
}


void audioloop::setVolumeNoMtx(uint8_t volume)
{
    if(volume > 100){
        volume = 100;
    }
    emit NewAudioVolume(volume);

    if(this->Muted==true){
        CurrentVolume = volume;
        return;
    }

    double imvol=0;


    if(0 != elem){
        imvol = volume * max;
        imvol = imvol / 100;
        snd_mixer_selem_set_playback_volume_all(elem, imvol);
    } else {
        /* Something went wrong */
    }


    this->CurrentVolume = volume;


}

uint8_t audioloop::getVolumeNoMtx(){


    uint8_t level=0;
    float imlevel=0;
    long value;



    if( 0 != elem ){
        snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &value);

        imlevel = value *100;
        imlevel= imlevel / max;
        level = round(imlevel);
        if(level>100){
            level=100;
        }
    } else {
        level = 0;
    }



    if(this->Muted==true){
            return CurrentVolume;
    }


    return level;

}

void audioloop::setVolume(uint8_t Volume){
 //   fprintf(stderr, "Lock Audio Mtx @ setVolume\n\r");
    audio_mutex.lock();

        this->setVolumeNoMtx(Volume);

     audio_mutex.unlock();
 //    fprintf(stderr, "Unlock Audio Mtx @ setVolume\n\r");
}

uint8_t audioloop::getVolume( void ){
    uint8_t volume = 0;
 //    fprintf(stderr, "Lock Audio Mtx @ getVolume\n\r");
    audio_mutex.lock();

      volume =  this->getVolumeNoMtx();

    audio_mutex.unlock();
 //   fprintf(stderr, "Unlock Audio Mtx @ setVolume\n\r");
    return volume;
}

void audioloop::setMute(){
 //   fprintf(stderr, "Lock Audio Mtx @ setMute\n\r");
   audio_mutex.lock();

    this->setMuteNoMtx();

   audio_mutex.unlock();
 //  fprintf(stderr, "Unlock Audio Mtx @ setMute\n\r");

}

void audioloop::setMuteNoMtx( void ){


    SavedVolume = CurrentVolume;
    this->setVolumeNoMtx(0);
    this->Muted = true;
    this->setVolumeNoMtx(SavedVolume);
    emit NewMusteStaus(true);

}


void audioloop::setUnmute(){
 //   fprintf(stderr, "Lock Audio Mtx @ setUnmute\n\r");
   audio_mutex.lock();

      this->setUnmuteNoMtx();

   audio_mutex.unlock();
 //  fprintf(stderr, "Unlock Audio Mtx @ setUnmute\n\r");

}

void audioloop::setUnmuteNoMtx(){

    this->Muted = false;
    this->setVolumeNoMtx(CurrentVolume);
    emit NewMusteStaus(false);

}
bool audioloop::getMute( void ){
    return this->Muted;
}

uint8_t audioloop::IncreaseVolume( void ){
    /* increase volume */
  //  fprintf(stderr, "Lock Audio Mtx @ IncreaseVolume\n\r");
    audio_mutex.lock();
    uint8_t volume = getVolumeNoMtx();
    if(volume<100){
        volume++;
        setVolumeNoMtx(volume);
    }
    audio_mutex.unlock();
   // fprintf(stderr, "Unlock Audio Mtx @ IncreaseVolume\n\r");
    return volume;
}

uint8_t audioloop::DecreaseVolume( void ){
   // fprintf(stderr, "Lock Audio Mtx @ DecreaseVolume\n\r");
    audio_mutex.lock();
    uint8_t volume = getVolumeNoMtx();
    if(volume>0){
        volume--;
        setVolumeNoMtx(volume);
    }
    audio_mutex.unlock();
    //fprintf(stderr, "Unlock Audio Mtx @ DecreaseVolume\n\r");
    return volume;
}

bool audioloop::toggleMute( void ){
    bool Muted= false;
     //fprintf(stderr, "Lock Audio Mtx @ toggleMute\n\r");
    audio_mutex.lock();
    if(this->Muted == true){
        setUnmuteNoMtx();
        Muted = false;
    } else {
        setMuteNoMtx();
        Muted = true;
    }
     audio_mutex.unlock();
     //fprintf(stderr, "Unlock Audio Mtx @ toggleMute\n\r");
    return Muted;
}
