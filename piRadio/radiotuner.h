#ifndef RADIOTUNER_H
#define RADIOTUNER_H

#include <stdint.h>
#include <linux/videodev2.h>
#include <QThread>
#include <QString>
#include <rds_decoder.h>

class radiotuner: public QObject
{
 Q_OBJECT

public:
    typedef enum Modulation {
        AM =0 ,
        SW,
        FM,
        ANY
    }Modulation_t;

    typedef enum {
        FREQFAIL=0,
        FREQOK,
        FREQOUTOFBAND,
        TUNERBUSY,
        TUNERFAIL
    } Tunerresult_t;

    explicit radiotuner( const char* device,  QObject *parent = nullptr ); /* Blocks untill tuner is open */
    ~radiotuner( void );
    Tunerresult_t changefrequency( uint32_t frequency ); /* Blocks till change is done */
    uint32_t getfrequency( void );                        /* Blocking Read */
    Modulation_t getmodulation( void );                   /* Blocking Read */
    void TunerClose();
    int radio_has_rds( void );      /* We don't neeed to block */
    int GetSiganlStrenght( void );  /* Blocking Read */
    void AudioMute( void );         /* Blocking Write */
    void AudioUnmute( void );       /* Blocking Write */
    int HasStereoReception( void ); /* Blocking Read */
    bool rds_avail( void );

    void Seek( bool upward, bool warp); /* Blocks untill seek done */
    bool SeekActive( void );
    bool GetRdsTitle( QString* QStrPtr );
    bool GetRdsStation ( QString* QStrPtr );

    rds_decoder* decoder = 0;
    Q_SIGNALS:

    void onRDSMessage( uint8_t Byte0, uint8_t Byte1, uint8_t Byte2 );
    void onRDSGroup(uint16_t BlockA, uint16_t BlockB, uint16_t BlockC, uint16_t BlockD );

    private Q_SLOTS:

    void RDSMessageHelper(uint8_t Byte0, uint8_t Byte1, uint8_t Byte2);
    void RDSGroupHelper(uint16_t BlockA, uint16_t BlockB, uint16_t BlockC, uint16_t BlockD );


    private:
    v4l2_frequency_band* findfreqband(uint32_t freq);
    void registerband( v4l2_frequency_band* band );
    char rds_data_buf[3];
    int radio_get_rds_data( void );
    int DoSeek(int fdradiodev, bool upward, bool warp);
    void DoFrequencyUpdate ( void );
    void DoReadRSSI( void );





};

#endif // RADIOTUNER_H
