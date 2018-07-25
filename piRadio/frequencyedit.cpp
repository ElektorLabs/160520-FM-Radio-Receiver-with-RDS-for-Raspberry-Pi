#include "frequencyedit.h"
#include "eventfilterforkeys.h"
#include "ui_frequencyedit.h"

#define FREQ_LOW ( 87500000)
#define FREQ_HIGH ( 180000000)

bool sub_mhz=false;
uint8_t sub_mhz_div=100;
eventFilterForKeys* evfk = nullptr;

frequencyedit::frequencyedit(uint32_t Frequency, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::frequencyedit)
{
    ui->setupUi(this);
    /* We set up the current frequency */
    this->update_frequnecy(Frequency);
    evfk = new eventFilterForKeys();
    this->installEventFilter(evfk);
    connect( evfk,&eventFilterForKeys::onKeyRelease,this,&frequencyedit::keyReleaseEvent);
    connect(evfk,&eventFilterForKeys::onKeyPress,this,&frequencyedit::keyPressEvent);
    /* We only enable 1,8,9 at the beginning */
    ui->BTN_0->setEnabled(true);
    ui->BTN_1->setEnabled(true);
    ui->BTN_2->setEnabled(true);
    ui->BTN_3->setEnabled(true);
    ui->BTN_4->setEnabled(true);
    ui->BTN_5->setEnabled(true);
    ui->BTN_6->setEnabled(true);
    ui->BTN_7->setEnabled(true);
    ui->BTN_8->setEnabled(true);
    ui->BTN_9->setEnabled(true);
    ui->BTN_COLON->setEnabled(true);

    if(true == Frequency_Valid()){
        ui->BTN_OKAY->setEnabled(false);
        ui->BTN_OKAY->setVisible(false);

    } else {
        ui->BTN_OKAY->setEnabled(true);
        ui->BTN_OKAY->setVisible(true);
    }



}

frequencyedit::~frequencyedit()
{
    delete ui;
}

void frequencyedit::update_frequnecy(uint32_t frequency){
    CurrentFrequency=frequency/50000;
    CurrentFrequency = CurrentFrequency * 5 ;
    Frequency_Mhz = ( CurrentFrequency / 100 );
    Frequency_kHz = CurrentFrequency - (Frequency_Mhz*100);
    sub_mhz=false;
    sub_mhz_div=100;

    ui->Frequency->setText( QString::number(Frequency_Mhz)+'.'+QString::number(Frequency_kHz/10) + " MHz");
    Frequency_kHz =0;
    Frequency_Mhz=0;

}




void frequencyedit::on_BTN_0_clicked()
{
     process_digit(0);
}

void frequencyedit::on_BTN_1_clicked()
{
    process_digit(1);
}

void frequencyedit::on_BTN_2_clicked()
{
     process_digit(2);
}

void frequencyedit::on_BTN_3_clicked()
{
    process_digit(3);
}

void frequencyedit::on_BTN_4_clicked()
{
    process_digit(4);
}

void frequencyedit::on_BTN_5_clicked()
{
     process_digit(5);
}

void frequencyedit::on_BTN_6_clicked()
{
     process_digit(6);
}

void frequencyedit::on_BTN_7_clicked()
{
    process_digit(7);
}

void frequencyedit::on_BTN_8_clicked()
{
    process_digit(8);
}

void frequencyedit::on_BTN_9_clicked()
{
    process_digit(9);
}

void frequencyedit::process_digit(uint8_t digit){

    if( sub_mhz == false ){
        if(digit<10){
            Frequency_Mhz=Frequency_Mhz*10;
            Frequency_Mhz+=digit;
        }
    } else {
           if(sub_mhz_div == 10 ){
               if(digit<5){
                   digit=0;
               } else {
                   digit=5;
               }

           }

           if(sub_mhz_div == 100){
               Frequency_kHz+=digit*sub_mhz_div;
               sub_mhz_div = 10 ;

           } else if ( sub_mhz_div == 10){
               Frequency_kHz+=digit*sub_mhz_div;
               sub_mhz_div = 1 ;
           } else {
               /* Do nothiing */
           }
    }
    ui->Frequency->setText( QString::number(Frequency_Mhz)+'.'+QString::number(Frequency_kHz/10) + " MHz");
    if(Frequency_Valid()==true){
         ui->BTN_OKAY->setEnabled(true);
         ui->BTN_OKAY->setVisible(true);
    } else {
        ui->BTN_OKAY->setEnabled(false);
        ui->BTN_OKAY->setVisible(false);
    }

}

void frequencyedit::process_seperator( void ){
        sub_mhz=true;
        sub_mhz_div=100;

}


void frequencyedit::on_BTN_OKAY_clicked()
{
     /* If the requency is valide we need to set it */
    uint32_t NewFrequency = Frequency_Mhz*1000000+Frequency_kHz*1000;

     this->hide();
    if( (NewFrequency >=FREQ_LOW) && ( NewFrequency <=FREQ_HIGH) ){
     emit FrequencyChanged(NewFrequency);
    }
}

void frequencyedit::on_BTN_ABORT_clicked()
{
     this->hide();
}

void frequencyedit::on_BTN_COLON_clicked()
{
   process_seperator();
}

void frequencyedit::on_BTN_BACK_clicked()
{
    Frequency_kHz=0;
    Frequency_Mhz=0;
    sub_mhz=false;
    sub_mhz_div=100;
    ui->Frequency->setText( QString::number(Frequency_Mhz)+','+QString::number(Frequency_kHz/10) );
    if(Frequency_Valid()==true){
         ui->BTN_OKAY->setEnabled(true);
         ui->BTN_OKAY->setVisible(true);
    } else {
        ui->BTN_OKAY->setEnabled(false);
        ui->BTN_OKAY->setVisible(false);
    }
}

void frequencyedit::keyPressEvent(QKeyEvent *event){

    switch(event->key() ){

        default:{

        } break;
    }

     event->accept();
}

void frequencyedit::keyReleaseEvent(QKeyEvent *event){

    if(this->isVisible()==true){
        switch(event->key()){
            case Qt::Key_0:{
                process_digit(0);
            } break;

            case Qt::Key_1:{
                process_digit(1);
            } break;

            case Qt::Key_2:{
                process_digit(2);
            } break;


            case Qt::Key_3:{
                process_digit(3);
            } break;


            case Qt::Key_4:{
                process_digit(4);
            } break;


            case Qt::Key_5:{
                process_digit(5);
            } break;


            case Qt::Key_6:{
                process_digit(6);
            } break;


            case Qt::Key_7:{
                process_digit(7);
            } break;


            case Qt::Key_8:{
                process_digit(8);
            } break;

            case Qt::Key_9:{
                process_digit(9);
            } break;

            case Qt::Key_Stop:
            case Qt::Key_Comma:{
                process_seperator();
            } break;

            case Qt::Key_Backspace:{
                on_BTN_BACK_clicked();
            } break;

            case Qt::Key_Enter:{
                uint32_t NewFrequency = Frequency_Mhz*1000000+Frequency_kHz*1000;

                this->hide();
               if( (NewFrequency >=FREQ_LOW) && ( NewFrequency <=FREQ_HIGH) ){
                emit FrequencyChanged(NewFrequency);
               }
            } break;

            case Qt::Key_E:
            case Qt::Key_Escape:{
                this->hide();
            }

            default:{

            } break;


        }
        event->accept();
    }
}

bool frequencyedit::Frequency_Valid( void ){
    bool Valid = false;
    uint32_t NewFrequency = Frequency_Mhz*1000000+Frequency_kHz*1000;


    if( (NewFrequency >=FREQ_LOW) && ( NewFrequency <=FREQ_HIGH) ){
     Valid=true;
    }
    return Valid;
}





