#ifndef RDS_DECODER_H
#define RDS_DECODER_H

#include <QObject>
#include "QFuture"
#include <QtConcurrent/QtConcurrent>

class rds_decoder : public QThread
{
    /* This is the rds decoder, currently only decoding the stationname and text */
    Q_OBJECT
public:
    explicit rds_decoder(int fd);
    void run();
    void reset();
    void end();
    bool RDS_Found( void );
    bool ReadRdsTitle( QString* QStrPtr );
    bool ReadRdsStation ( QString* QStrPtr );
signals:
    void NewRDSMessage( uint8_t Block, uint8_t lsb, uint8_t msb );
    void NewRDSGroup(uint16_t BlockA, uint16_t BlockB, uint16_t BlockC, uint16_t BlockD );
//    void NewRDSText(QString Text);
//    void NewRDSStation(QString Station);

public slots:
    /* Decoder reset */

private:

    typedef enum {
        GET_BLOCK_A,
        GET_BLOCK_B,
        GET_BLOCK_C,
        GET_BLOCK_D,
        BLOCK_DECODE
    } RECEPTION_STATE;


    enum class CodeTable {
      G0, G1, G2
    };

    QString RDSCharString(uint8_t code, CodeTable codetable);

    QFuture<void> rds_worker;
    void decode_data( void );
    void decode_group( uint16_t Group[]);
};

#endif // RDS_DECODER_H
