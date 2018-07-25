#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <linux/videodev2.h>
#include <alsa/asoundlib.h>

#include "QMutex"
#include "QFuture"
#include <QtConcurrent/QtConcurrent>

#include "radiotuner.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define logit(priority, fmt, args...) do {		\
        fprintf(stderr, fmt, ##args);		\
} while (0)

/* We use a linked list for the frequencys */
typedef struct {
    void* ptrprevband;
    void* ptrnextband;
    v4l2_frequency_band* band;
} FreqBand_t;

FreqBand_t* ptrbandlist = nullptr;

v4l2_tuner tuner;
v4l2_capability tunercapabilitys;

int fdradiodevice=0;

QThread *SeekThread;
QThread *RdsThread;
QMutex *SeekSettingsMutex;

struct v4l2_hw_freq_seek seeksettings;
QFuture<int> SeekWorker;
QFuture<void> FrequencyWorker;
QFuture<void> RssiWorker;
QFuture<void> RDSWorker;

uint32_t CurrentFrequencyHz=0;
uint32_t CurrentRSSI=0;


radiotuner::radiotuner(const char* device, QObject *parent):  QObject(parent)
{

    int32_t Result=-1;
    const u_int32_t requieredcapabilitys = ( V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_STEREO | V4L2_TUNER_CAP_FREQ_BANDS);
    v4l2_frequency_band readoutband; /* used for temporary storage of band info */
   logit(LOG_WARNING,"Open Tuner");
   fdradiodevice = open( device , O_RDONLY  );
    logit(LOG_WARNING,"Open returned");
   if(-1 == fdradiodevice){
       /* We have an error */
       switch(errno){
           case EACCES:{
                logit(LOG_WARNING,"Tuner: EACCES");
           } break;

           case EBUSY:{
                 logit(LOG_WARNING,"Tuner: EBUSY");
           } break;

           case ENXIO:{
                logit(LOG_WARNING,"Tuner: ENXIO");
           } break;

           case ENOMEM:{
                logit(LOG_WARNING,"Tuner: ENOMEM");
           } break;

           case EMFILE:{
                logit(LOG_WARNING,"Tuner: EMFILE");
           } break;

           case ENFILE:{
                logit(LOG_WARNING,"Tuner: ENFILE");
           } break;

           default:{
                logit(LOG_WARNING,"Tuner: error %i", errno);
           } break;
       }
   } else {
       /* Next is to read the tuner band capabilies */
       ioctl( fdradiodevice , VIDIOC_G_TUNER , &tuner );

       /* Sainirycheck if the device has an tuner and if it is the correct one */
       if( requieredcapabilitys != ( tuner.capability & requieredcapabilitys) ){
        /* We have missing capabiliys */

       } else {
           ioctl( fdradiodevice, VIDIOC_QUERYCAP, &tunercapabilitys);

           printf("V4L device, driver='%s', card='%s', seek=%d\n",
                   tunercapabilitys.driver,
                   tunercapabilitys.card,
                   tunercapabilitys.capabilities & V4L2_CAP_HW_FREQ_SEEK);


           CLEAR( readoutband );


           readoutband.tuner = tuner.index;
           readoutband.type = tuner.type;
           readoutband.index = 0;



           Result = ioctl( fdradiodevice, VIDIOC_ENUM_FREQ_BANDS ,&readoutband);
           while(0 == Result ){
                 v4l2_frequency_band* ptrband = new  v4l2_frequency_band;
                 memcpy(ptrband,&readoutband,sizeof(readoutband) );
                 this->registerband(ptrband);

                 CLEAR( readoutband );
                 readoutband.tuner = tuner.index;
                 readoutband.type = tuner.type;
                 readoutband.index = ptrband->index+1;
                 Result = ioctl( fdradiodevice, VIDIOC_ENUM_FREQ_BANDS ,&readoutband);
           }
          decoder = new rds_decoder(fdradiodevice);
          decoder->start();
          decoder->setPriority(QThread::HighPriority);
          //    connect(evf,&eventFilterForKeys::onKeyPress,this,&MainWindow::on_keyPressEvent);
          QObject::connect(decoder,&rds_decoder::NewRDSGroup,this,&radiotuner::RDSGroupHelper);
          QObject::connect(decoder,&rds_decoder::NewRDSMessage,this,&radiotuner::RDSMessageHelper);

        }


   }
   SeekSettingsMutex =new QMutex;

}

radiotuner::~radiotuner(){
    if (fdradiodevice >= 0)
    {
        close(fdradiodevice);
        fdradiodevice = -1;
    }
    if(decoder != nullptr){
        decoder->exit(0);
    }

}

void radiotuner::registerband( v4l2_frequency_band* band ){
    FreqBand_t* ptrband = new FreqBand_t;
    FreqBand_t* currfreqband = ptrbandlist;

    ptrband->band = band;
    ptrband->ptrnextband = nullptr;

    if( nullptr == ptrbandlist ){
        ptrbandlist = ptrband;
        ptrbandlist->ptrprevband = nullptr;
    } else {
        /* Run along the list and find the last entry */
        while ( nullptr != currfreqband->ptrnextband  ){
            currfreqband = (FreqBand_t*)(currfreqband->ptrnextband);
        }
        currfreqband->ptrnextband = ptrband;
    }

}

radiotuner::Tunerresult_t radiotuner::changefrequency( uint32_t frequency ){

    struct v4l2_frequency freqeuncy_demand;

    if( 0 != (tuner.capability & V4L2_TUNER_CAP_1HZ ) ){
      freqeuncy_demand.frequency = (u_int32_t)(frequency);
    } else if( 0 == (tuner.capability & V4L2_TUNER_CAP_LOW ) ){
      freqeuncy_demand.frequency = (u_int32_t)( (frequency) / (62.5*1000) );
    } else {
      freqeuncy_demand.frequency = (u_int32_t)( (frequency) / (62.5) );
    }
    freqeuncy_demand.tuner = tuner.index;
    freqeuncy_demand.type = tuner.type;
    CLEAR(freqeuncy_demand.reserved);

    /* This is a hard fm only limit */
    if((frequency >=64000000) && ( frequency <=108000000)){

        if(fdradiodevice > 0){
            /*
             * Here we need a call to the v4l api of the radio
             */
            ioctl(fdradiodevice,VIDIOC_S_FREQUENCY ,&freqeuncy_demand);
        }

    decoder->reset();

        return FREQOK;

    } else {

        return FREQFAIL;
    }


}

void radiotuner::DoFrequencyUpdate( void ){
    /* We will read the frequency fresh from the device */
    int res;
    unsigned long freq;
    struct v4l2_frequency v_frequency;

    v_frequency.tuner = tuner.index;
    v_frequency.type = tuner.type;
    v_frequency.frequency=0;
    CLEAR(v_frequency.reserved);

    if (fdradiodevice < 0){
       return;
    }

    res = ioctl(fdradiodevice, VIDIOC_G_FREQUENCY, &v_frequency);

    if(res < 0){
        switch( errno ){
        case EINVAL:{
            printf("VIDIOC_G_FREQUENCY failed: EINVAIL");
        } break;

        case EBUSY:{
            printf("VIDIOC_G_FREQUENCY failed: EBUSY");
        } break;

        default:{
            printf("VIDIOC_G_FREQUENCY failed: default");
        } break;

        }
        freq = 0.0;
    } else {

        if( 0 != (tuner.capability & V4L2_TUNER_CAP_1HZ ) ){
          freq = v_frequency.frequency;
        } else if( 0 == (tuner.capability & V4L2_TUNER_CAP_LOW ) ){
          freq = v_frequency.frequency*62.5*1000;
        } else {
          freq = v_frequency.frequency*62.5;
        }
    }
    CurrentFrequencyHz=freq;
}

u_int32_t radiotuner::getfrequency( void ){
    if(true == SeekWorker.isFinished() ){
         FrequencyWorker = QtConcurrent::run(this,&radiotuner::DoFrequencyUpdate);
    } else {
       /* Still reading */
    }
    /* We ignore the result here and leave for now */
    return CurrentFrequencyHz;
}

radiotuner::Modulation_t radiotuner::getmodulation( void ){
    /* We will read the frequency fresh from the device */
    Modulation_t mod = ANY;
    v4l2_frequency_band* current_band  = findfreqband( getfrequency() );
    if(nullptr != current_band ){
            switch( current_band->modulation){
                case V4L2_BAND_MODULATION_AM:{
                   mod = AM;
                } break;

                case V4L2_BAND_MODULATION_FM:{
                   mod = FM;
                } break;

                default:{
                   mod = ANY;
                }

            }
    } else {
        mod = ANY;
    }
    return mod;
}

int radiotuner::radio_has_rds()
{
    return (tunercapabilitys.capabilities & V4L2_CAP_RDS_CAPTURE) != 0;
}

void radiotuner::DoReadRSSI( void ){
    if(fdradiodevice > 0){
        if (ioctl(fdradiodevice, VIDIOC_G_TUNER, &tuner) < 0)
        {

        }
    }
    CurrentRSSI = tuner.signal;
}

int radiotuner::GetSiganlStrenght()
{
    if(true == RssiWorker.isFinished() ){
        RssiWorker = QtConcurrent::run(this,&radiotuner::DoReadRSSI);

    } else {

    }
    /* We ignore the result here and leave for now */

    return CurrentRSSI;
}

void radiotuner::AudioMute(){

    int res;
    struct v4l2_control v_control;
    v_control.id = V4L2_CID_AUDIO_MUTE;
    v_control.value = 1;
    res = ioctl(fdradiodevice, VIDIOC_S_CTRL, &v_control);
    if( res > 0 )
    {

    }

}

void radiotuner::AudioUnmute(){

    int res;
    struct v4l2_control v_control;
    v_control.id = V4L2_CID_AUDIO_MUTE;
    v_control.value = 0;
    res = ioctl(fdradiodevice, VIDIOC_S_CTRL, &v_control);
    if( res > 0 )
    {

    }
}

int radiotuner::HasStereoReception()
{
        struct v4l2_audio va;
        va.mode=-1;

        if (fdradiodevice < 0)
            return -1;

        if (ioctl (fdradiodevice, VIDIOC_G_AUDIO, &va) < 0)
            return -1;

        if (va.mode == V4L2_AUDCAP_STEREO)
            return 1;
        else
            return 0;
}


v4l2_frequency_band* radiotuner::findfreqband(uint32_t freq){
    /* We try to find the band the frequency is in */
    /* But fist step is to get the frequency into the tuner f_uinit */

    uint32_t tuner_freq = 0;
    FreqBand_t* current_band = ptrbandlist;
    v4l2_frequency_band* found_band = nullptr;

    if( 0 != (tuner.capability & V4L2_TUNER_CAP_1HZ ) ){
      tuner_freq = freq;
    } else if( 0 == (tuner.capability & V4L2_TUNER_CAP_LOW ) ){
      tuner_freq = freq/(62500);
    } else {
      tuner_freq = freq/62.5;
    }

    /* Next is a walk through the bandlist */
    while(nullptr != current_band){
       if ( ( current_band->band->rangelow <= tuner_freq ) && ( current_band->band->rangehigh >= tuner_freq ) ){
            /* We have a match */
            found_band = current_band->band;
            break;
        } else {
            /* We need the next band */
            current_band = (FreqBand_t*)current_band->ptrnextband;
        }

    }

    return found_band;

}

bool radiotuner::SeekActive( void ){
    if(SeekWorker.isFinished()==false){
        return true;
    } else {
        return false;
    }
}

void radiotuner::Seek( bool upward, bool warp){
    if(true == SeekWorker.isFinished() ){
        SeekWorker = QtConcurrent::run(this,&radiotuner::DoSeek,fdradiodevice,upward,warp);
         fprintf(stderr, "Seek Started");
    } else {
         fprintf(stderr, "Seek running");
    }
    /* We ignore the result here and leave for now */
    decoder->reset();
}


int radiotuner::DoSeek(int fdradiodev, bool upward, bool warp){


    int res=0;

    v4l2_frequency_band* bandptr=nullptr;

    bandptr = findfreqband(getfrequency());

    if (fdradiodev < 0){
        return -1;
    }

    if(nullptr == bandptr ){
        return -2;
    }

   if(false == SeekSettingsMutex->tryLock(0)){
    return -1;
   } else {
    seeksettings.rangehigh = 0;
    seeksettings.rangelow = 0;

    if(true==upward){
        seeksettings.seek_upward=1;
    } else {
        seeksettings.seek_upward=0;
    }

    if(true==warp){
        seeksettings.wrap_around=1;
    } else {
        seeksettings.wrap_around=0;
    }

    seeksettings.spacing=0;
    seeksettings.tuner = tuner.index;
    seeksettings.type = tuner.type;

    res = ioctl(fdradiodevice, VIDIOC_S_HW_FREQ_SEEK , &seeksettings);
    if( res < 0 )
    {
        switch ( errno ){
            case EINVAL:{
               printf("Seek:EINVAIL");
            } break;

            case EAGAIN:{
                printf("Seek:EAGAIN");
            } break;

            case ENODATA:{
                printf("Seek:ENODATA");
            } break;

            case EBUSY:{
                printf("Seek:EBUSY");
            } break;

            default:{
                printf("Seek:default error");
            }

        }
    }

  }
  SeekSettingsMutex->unlock();
  return 0;

}


bool radiotuner::GetRdsTitle( QString* QStrPtr ){
    if(decoder != nullptr){
        return decoder->ReadRdsTitle( QStrPtr);
    } else{
      *QStrPtr="";
      return true;
    }
}
bool radiotuner::GetRdsStation ( QString* QStrPtr ){
    if(decoder != nullptr){
        return decoder->ReadRdsStation( QStrPtr);
    } else{
        *QStrPtr="";
        return true;
    }
}

bool radiotuner::rds_avail( void ){
    if(decoder != nullptr ){
        return decoder->RDS_Found();
    } else {
        return false;
    }

}

void radiotuner::TunerClose( void ){
    /* clean */
    uint8_t waitcnt=0;
    if(decoder != nullptr){
       decoder->end();
       while(decoder->isRunning()==true){
        QThread::sleep(1);
        waitcnt++;
        if(waitcnt>10){
            decoder->terminate();
        }
       }

    }
    if(fdradiodevice != 0){
        close(fdradiodevice);
    }

}

void radiotuner::RDSMessageHelper(uint8_t Byte0, uint8_t Byte1, uint8_t Byte2){
    emit onRDSMessage(Byte0,Byte1,Byte2);
}

void radiotuner::RDSGroupHelper(uint16_t BlockA, uint16_t BlockB, uint16_t BlockC, uint16_t BlockD ){
    emit onRDSGroup(BlockA,BlockB,BlockC,BlockD);  
}




