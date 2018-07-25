#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "audiohelper.h"

bool ShutdownVar = false;
void audiohelper::run(){
    #define BUFSIZE (1024 * 16)

    snd_pcm_t *playback_handle, *capture_handle;
    int audio_buffer[BUFSIZE * 2];
    int audio_buffer_size=0;
    int err;
    bool restart = false;
    bool open_rec = false;
    bool open_ply = false;


    while ( false == ShutdownVar ){
        open_rec = false;
        open_ply = false;
        fprintf(stderr, "Wait 1s for audio to be ready");
         QThread::msleep(950);
         fprintf(stderr, "Try to start audio loop");
         restart = false;

         if( restart == false ){
             if ((err = open_stream(&playback_handle, "hw:0,0", SND_PCM_STREAM_PLAYBACK)) < 0){
                 fprintf(stderr, "cannot open playback device for use(%s)\n",
                      snd_strerror(err));
                 restart = true ;
             } else {
                 open_ply = true;
             }
         }

         if( restart == false ){
             if ((err = open_stream(&capture_handle, "hw:0,1", SND_PCM_STREAM_CAPTURE)) < 0){
                 fprintf(stderr, "cannot open playback capture device for use(%s)\n",
                      snd_strerror(err));
                  restart = true ;
             } else {
                 open_rec = true;
             }
         }


         if( restart == false ){
             if ((err = snd_pcm_prepare(playback_handle)) < 0) {
                 fprintf(stderr, "cannot prepare audio interface for use(%s)\n",
                      snd_strerror(err));
                  restart = true ;
             }
         }

         if( restart == false ){
             if ((err = snd_pcm_start(capture_handle)) < 0) {
                 fprintf(stderr, "cannot prepare audio interface for use(%s)\n",
                      snd_strerror(err));
                  restart = true  ;
             }
         }

         memset(audio_buffer, 0, sizeof(audio_buffer));

         while ( ( restart == false ) && ( ShutdownVar == false ) ) {
             int avail;


             if ((err = snd_pcm_wait(playback_handle, 1000)) < 0) {
                 fprintf(stderr, "poll failed(%s)\n", strerror(errno));
                restart= true;

             } else {

                 avail = snd_pcm_avail_update(capture_handle);
                 if (avail > 0) {
                     if (avail > BUFSIZE)
                         avail = BUFSIZE;

                     snd_pcm_readi(capture_handle, audio_buffer, avail);
                     audio_buffer_size = avail;
                 }

                 avail = snd_pcm_avail_update(playback_handle);
                 if (avail > 0) {
                     if (avail < audio_buffer_size)
                         audio_buffer_size = avail;

                     snd_pcm_writei(playback_handle, audio_buffer, audio_buffer_size);
                     snd_pcm_start(playback_handle);

                 }
                    active=true;
             }
         }
        if(open_ply==true){
             snd_pcm_close(playback_handle);
        }
        if(open_rec==true){
             snd_pcm_close(capture_handle);
        }
    }

}

void audiohelper::Shutdown( void ){
    ShutdownVar = true;
}

int audiohelper::open_stream(snd_pcm_t **handle, const char *name, snd_pcm_stream_t dir)
{
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    unsigned int rate = 44100;
    /* We want 500 to 1000 ms delay in the audio */
    snd_pcm_uframes_t periods=4;          /* Number of fragments/periods */
    snd_pcm_uframes_t periodsize = 8196;    /* Periodsize (bytes) */

    const char *dirname = (dir == SND_PCM_STREAM_PLAYBACK) ? "PLAYBACK" : "CAPTURE";
    int err;

    if ((err = snd_pcm_open(handle, name, dir, 0)) < 0) {
        fprintf(stderr, "%s (%s): cannot open audio device (%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot allocate hardware parameter structure(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_any(*handle, hw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot initialize hardware parameter structure(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "%s (%s): cannot set access type(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_format(*handle, hw_params, format)) < 0) {
        fprintf(stderr, "%s (%s): cannot set sample format(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(*handle, hw_params, &rate, NULL)) < 0) {
        fprintf(stderr, "%s (%s): cannot set sample rate(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, 2)) < 0) {
        fprintf(stderr, "%s (%s): cannot set channel count(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }

    /* Set number of periods. Periods used to be called fragments. */
    if( (err=snd_pcm_hw_params_set_periods(*handle, hw_params, periods, 0)) < 0 )
    {
        fprintf(stderr, "Error setting periods.\n");

        return(err);
    }

    snd_pcm_uframes_t size = (periodsize * periods) >> 2;
    if( (err = snd_pcm_hw_params_set_buffer_size_near( *handle, hw_params, &size )) < 0)
    {
        fprintf(stderr, "Error setting buffersize: [%s]\n", snd_strerror(err) );

        return(err);
    }
    else
    {
        printf("Buffer size = %lu\n", (unsigned long)size);
    }

    if ((err = snd_pcm_hw_params(*handle, hw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot set parameters(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }



    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot allocate software parameters structure(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }
    if ((err = snd_pcm_sw_params_current(*handle, sw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot initialize software parameters structure(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }
    if ((err = snd_pcm_sw_params_set_avail_min(*handle, sw_params, 1024)) < 0) {
        fprintf(stderr, "%s (%s): cannot set minimum available count(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }
    if ((err = snd_pcm_sw_params_set_start_threshold(*handle, sw_params, 0U)) < 0) {
        fprintf(stderr, "%s (%s): cannot set start mode(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }
    if ((err = snd_pcm_sw_params(*handle, sw_params)) < 0) {
        fprintf(stderr, "%s (%s): cannot set software parameters(%s)\n",
            name, dirname, snd_strerror(err));
        return err;
    }


    return 0;
}

bool audiohelper::GetActive( void ){
    return this->active;
}

