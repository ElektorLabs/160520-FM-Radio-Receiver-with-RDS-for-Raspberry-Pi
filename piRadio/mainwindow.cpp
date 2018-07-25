#include "mainwindow.h"
#include "frequencyedit.h"
#include "ui_mainwindow.h"
#include "QMutex"
#include "QTimer"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QFile>
#include <QJsonParseError>
#include <qendian.h>

#include "eventfilterforkeys.h"

#include "radiotuner.h"
#include "audioloop.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QTime>

#include "websocketconnector.h"


#define FM_FREQ_MIN ( 87500000)
#define FM_FREQ_MAX ( 108000000 )

#define FREQ_STEP_MIN   ( 50000 )
#define FREQ_TICK_STEP  ( 50 )

#define FREQ_TICK_MIN ( 100 )
#define FREQ_TICK_START ( 250 )

#define STRATUP_STATIONPAGES ( 4 )

QMutex FrequencyMutex;
QTimer* AutoscrollingTimer ;
QTimer* DisplayUpdateTimer ;
QTimer* RemoteStationInputTimer ;

QTimer* StoreStationTimer;
QTimer* ExitToDesktopTimer;

radiotuner* radio;
audioloop* audiodev;

frequencyedit* frqeditwdn;

static uint32_t RadioFrequency = FM_FREQ_MIN ;
static uint32_t step=FREQ_STEP_MIN;


WebSocketConnector *server =nullptr;

static uint32_t stepcount=0;
eventFilterForKeys* evf = nullptr;

int8_t RemoteLastDigit = -1;

uint8_t StoreButtonId=0;

typedef struct {
    bool used;
    uint32_t frequency;
    QString* StationName;
} Station_t;


#define STATIONPERPAGE ( 8 )

Station_t* StationMemory = nullptr;
QPushButton* StationMemoryButtonArray[ STATIONPERPAGE ];

QTime TimeSpan;

bool WillExitToDesktop = false;
uint32_t ConfiguredStationPages= STRATUP_STATIONPAGES ;

uint32_t CurrentStationPage=0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    this->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

    evf = new eventFilterForKeys();
    this->installEventFilter(evf);

    QJsonDocument doc;
    QJsonObject jObj;
    QJsonParseError json_error;
    ui->setupUi(this);
    AutoscrollingTimer = new QTimer ( this );
    radio=nullptr;
    printf("Start Audio");

    audiodev = new audioloop();
    StationMemoryButtonArray[0]=ui->M1;
    StationMemoryButtonArray[1]=ui->M2;
    StationMemoryButtonArray[2]=ui->M3;
    StationMemoryButtonArray[3]=ui->M4;
    StationMemoryButtonArray[4]=ui->M5;
    StationMemoryButtonArray[5]=ui->M6;
    StationMemoryButtonArray[6]=ui->M7;
    StationMemoryButtonArray[7]=ui->M8;

    server = new WebSocketConnector(8080, true);

    /* we load the settings from the config if there is any */
    QByteArray Data ;
    QFile file("config.json");
    if (true == file.open(QIODevice::ReadOnly | QIODevice::Text )){
        Data = file.readAll();
        doc=QJsonDocument::fromJson(Data,&json_error);
        file.close();
        jObj = doc.object();
    } else {
        /* We create a new file */
        QJsonObject recordObject;
        recordObject.insert("LastFrequency", QJsonValue((int)89000000));
        recordObject.insert("Volume", QJsonValue((int)0));
        QJsonDocument doc(recordObject);

        QFile file("config.json");
        if (true == file.open(QIODevice::WriteOnly | QIODevice::Text )){
            QTextStream out(&file);
            out << doc.toJson();
            file.close();
            jObj=recordObject;
        } else {

        }
    }
    QJsonValue Freq = jObj.value("LastFrequency");
    RadioFrequency=Freq.toInt(89000000);
    if(RadioFrequency<89000000){
        RadioFrequency=89000000;
    }

    if(RadioFrequency>108000000){
        RadioFrequency=108000000;
    }

    QJsonValue Vol = jObj.value("Volume");
    audiodev->setVolume(Vol.toInt(0));

    QJsonValue Mute = jObj.value("Mute");
    if(true == Mute.toBool(false) ){
        audiodev->setMute();
         ui->VOLUME_MUTE->setIcon(QIcon(":/icons/volume-mute.png"));

    } else {
        audiodev->setUnmute();
         ui->VOLUME_MUTE->setIcon(QIcon(":/icons/volume.png"));
    }

    QJsonValue StPages = jObj.value("StationPages");
    ConfiguredStationPages = StPages.toInt( STRATUP_STATIONPAGES );
    if(ConfiguredStationPages<1){
        ConfiguredStationPages=1;
    }

    StationMemory = new Station_t[STATIONPERPAGE * ConfiguredStationPages];

    for(uint32_t i=0; i < ( STATIONPERPAGE * ConfiguredStationPages ); i++){
        StationMemory[i].used=false;
        StationMemory[i].frequency=0;
        StationMemory[i].StationName = new QString("");;
    }


    /* Next is to load the station memory */
    QJsonValue StationValue = jObj.value("StationMemmory");
    QJsonArray StationArray = StationValue.toArray();
    for ( int32_t i=0;i<StationArray.size();i++){
        if( (i>=0) && (i <   (int32_t)(STATIONPERPAGE * ConfiguredStationPages) )){
            QJsonValue jv = StationArray.at(i);
            QJsonObject jObj = jv.toObject();

            StationMemory[i].used = jObj.value("Used").toBool(false);
            StationMemory[i].frequency = jObj.value("Frequency").toInt(0);
            StationMemory[i].StationName = new QString("");
            *StationMemory[i].StationName = jObj.value("Name").toString();
        } else {
            /* More stations than memory here */
        }


    }

    CurrentStationPage=0;
    if(ConfiguredStationPages<=1){
        ui->M4_2->setVisible(false);
    } else {
        ui->M4_2->setVisible(true);
    }

    ReloadButtons();

    DisplayUpdateTimer= new QTimer( this );
    connect(DisplayUpdateTimer,SIGNAL(timeout()), this, SLOT( updatedisplay() ) );
    DisplayUpdateTimer->start(250);

    frqeditwdn = new frequencyedit(RadioFrequency);
    frqeditwdn->hide();
    frqeditwdn->setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint );
    connect(frqeditwdn, &frequencyedit::FrequencyChanged, this, &MainWindow::UpdateRadioFreq);

    connect(evf,&eventFilterForKeys::onKeyPress,this,&MainWindow::on_keyPressEvent);
    connect(evf,&eventFilterForKeys::onKeyRelease,this,&MainWindow::on_keyReleaseEvent);

    StoreStationTimer = new QTimer(this);
    connect(StoreStationTimer,SIGNAL(timeout()), this, SLOT( StoreStation() ) );

    ExitToDesktopTimer = new QTimer(this);
    connect(ExitToDesktopTimer,SIGNAL(timeout()), this, SLOT( ShowExitToDesktop()) );

    RemoteStationInputTimer = new QTimer();
    RemoteStationInputTimer->setSingleShot(true);
    RemoteStationInputTimer->setInterval(1500);
    connect(RemoteStationInputTimer,SIGNAL(timeout()),this, SLOT(RemoteInputTimerElapsed()) );

    connect(audiodev,&audioloop::NewAudioVolume,server,&WebSocketConnector::SendNewVolume);

    ui->SaveIcon->setVisible(false);


}

MainWindow::~MainWindow()
{

    delete ui;
}

bool MainWindow::startRadio(){
    uint8_t counter=60;
    while(audiodev->getLoopready()==false){
        counter--;
        if(counter==0){
            return false;
        }
        QThread::msleep(500);
        fprintf(stderr,"Wait for Audio");
    }
    radio = new radiotuner("/dev/radio0");
    radio->changefrequency(RadioFrequency);
    UpdateRadioFreq(RadioFrequency);

    connect(radio->decoder,&rds_decoder::NewRDSGroup,server,&WebSocketConnector::onNewRDSGroup);
    connect(radio->decoder,&rds_decoder::NewRDSMessage,server,&WebSocketConnector::onNewRDSRawData);

    connect(server,&WebSocketConnector::AudioDecrease,audiodev,&audioloop::DecreaseVolume);
    connect(server,&WebSocketConnector::AudioIncrease,audiodev,&audioloop::IncreaseVolume);
    connect(server,&WebSocketConnector::AudioToggelMute,this,&MainWindow::on_VOLUME_MUTE_clicked);

    connect(server,&WebSocketConnector::FrequencyStepDown,this,&MainWindow::on_FREQ_DOWN_released);
    connect(server,&WebSocketConnector::FrequencyStepUp,this,&MainWindow::on_FREQ_UP_released);
    connect(server, &WebSocketConnector::FrequencySet, radio,&radiotuner::changefrequency);
    connect(server, &WebSocketConnector::FrequencySeek, radio, &radiotuner::Seek);

    connect(server, & WebSocketConnector::ChangeChannel, this, &MainWindow::LoadStationMemmory);


    return true;
}

/* We need to store how long we are doing this as we start after 3 seconds a seek */
void MainWindow::FreqDec(){

    stepcount++;
    if(stepcount>7){
        /* we do a seek */
        AutoscrollingTimer->stop();
        stepcount=0;
        if(radio!=nullptr){
            QFont Fnt = ui->radiodisplay_multiline->font();
            Fnt.setPointSize(36);
            ui->radiodisplay_multiline->setFont(Fnt);
            ui->radiodisplay_multiline->setText("Seek");
            radio->Seek(false,true);
        }
        return;
    }
    if(radio==nullptr){
        return;
    }
    /* We use 0.05 MHz steps */
    printf("FreqDec called");
    if(AutoscrollingTimer->interval()>FREQ_TICK_MIN){
        AutoscrollingTimer->setInterval(AutoscrollingTimer->interval()- FREQ_TICK_STEP );
    } else {
        if(step<FREQ_STEP_MIN){
            step=FREQ_STEP_MIN;
        }
    }
    if(true==FrequencyMutex.try_lock()){
        RadioFrequency-=step;
        if(RadioFrequency < FM_FREQ_MIN ){
            RadioFrequency = FM_FREQ_MIN ;
        }
        FrequencyMutex.unlock();
    }
    UpdateRadioFreq( RadioFrequency );

}

void MainWindow::FreqInc(){
    /* We use 0.05 MHz steps */
    stepcount++;
    if(stepcount>7){
        /* we do a seek */
        AutoscrollingTimer->stop();
        stepcount=0;
        if(radio!=nullptr){
            QFont Fnt = ui->radiodisplay_multiline->font();
            Fnt.setPointSize(36);
            ui->radiodisplay_multiline->setFont(Fnt);
            ui->radiodisplay_multiline->setText("Seek");
            radio->Seek(true,true);
        }
        return;
    }

    if(radio==nullptr){
        return;
    }
    printf("FreqInc called");
    if(AutoscrollingTimer->interval()>FREQ_TICK_MIN){
        AutoscrollingTimer->setInterval(AutoscrollingTimer->interval()- FREQ_TICK_STEP );
    } else {
        if(step<0.5){
            step+=FREQ_STEP_MIN;
        }
    }
    if(true==FrequencyMutex.try_lock()){
            RadioFrequency+=step;
            if(RadioFrequency > FM_FREQ_MAX ){
                RadioFrequency = FM_FREQ_MAX ;
            }
        FrequencyMutex.unlock();
    }
    UpdateRadioFreq( RadioFrequency );

}

void MainWindow::UpdateRadioFreq(uint32_t Frq){

    /* We build a nice text for the UI */
    if(radio!=nullptr){
        radio->changefrequency(Frq);

    }
    updatedisplay();

}


void MainWindow::on_FREQ_DOWN_pressed()
{
  /* We will start a timer that will cyclic also decrease the frequency */
  stepcount =0;
  step=FREQ_STEP_MIN;
  MainWindow::FreqDec();
  connect(AutoscrollingTimer,SIGNAL(timeout()), this, SLOT( FreqDec() ) );
  AutoscrollingTimer->start(FREQ_TICK_START);

}

void MainWindow::on_FREQ_DOWN_released()
{
    /* Stop any active timer and do a final update */
    AutoscrollingTimer->stop();
    AutoscrollingTimer->disconnect();
    step=0;

}


void MainWindow::on_FREQ_UP_pressed()
{
    stepcount =0;
    step=FREQ_STEP_MIN;
    MainWindow::FreqInc();
    connect(AutoscrollingTimer,SIGNAL(timeout()), this, SLOT( FreqInc() ) );
    AutoscrollingTimer->start(FREQ_TICK_START);
}

void MainWindow::on_FREQ_UP_released()
{
    /* Stop any active timer and do a final update */
    AutoscrollingTimer->stop();
    AutoscrollingTimer->disconnect();
    step=0;
}

void MainWindow::updatedisplay( ){
    static int32_t TextShift = 0;
    static uint8_t TextDelay=0;
    int32_t overflow_textwdn=0;
    static QString Buffer="";
    QString RadioText="";
    uint32_t frequency = 0.0;
    int32_t rssi = 0.0;
    uint8_t volume =0;
    if(radio==nullptr){
        return;
    }
    /* We read the current frequency and rssi form the tuner */
    if(radio!=nullptr){
    frequency = radio->getfrequency();
    rssi = radio->GetSiganlStrenght();
    } else {
        rssi = 0;
        frequency = RadioFrequency;
    }
    server->SendFrequency(frequency);
    server-> SendSignallevel(rssi/655);

    ui->RSSI->setValue(rssi/655);
    ui->SIGNALLEVEL->setText(QString("%1").arg(rssi/655));
    QString FreqString = QString::number(((double)frequency/1000000),'f',2)+" MHz";
    QString Station;
    if( true == radio->GetRdsStation(&Station) ){
        /* We have a new Stationtext */

    }
    if(TextDelay<255){
        TextDelay++;
    }
    /* here it becomes tricky */
    if(radio->SeekActive()==false){
        /* We need to wait for 1 second ( 4 cycels ) */
        if(true==radio->GetRdsTitle(&RadioText) ){
            TextDelay=0;
        }

        if(TextDelay == 8){
            TextShift=0;
            Buffer = RadioText;
        }

        if(TextDelay>=8){
             Buffer = RadioText;
        } else {
             RadioText=Buffer;
        }
        RadioText = RadioText.trimmed(); /* Remove whitespaces */

    /* We need to decide if we have to scroll the text or not */

    server->SendRDSStationName(Station);
    server->SendRDSText(RadioText);
    if(RadioText.length()>24){
        /* Scrolling is requiered */
        if( (TextDelay> (8+4) ) || ( TextDelay < 8) ){
             TextShift++;
        }
        /* Offset is TextShift / 2 -> 500ms step */
        if(TextShift>RadioText.length()-1){
            TextShift=0;
        }
       /* Sliding window for scrolling */
       /* We grab from position TextShift on 24 elements */
        overflow_textwdn = TextShift + 24 - RadioText.length();
        if(overflow_textwdn>0){
          RadioText = RadioText.mid(TextShift,24) + " " + RadioText.mid(0,overflow_textwdn);
        } else {
            RadioText = RadioText.mid(TextShift,24);
        }



    } else {
         TextShift=0;
    }

            if( (RadioText.length()<=0) && ( Station.length()<=0) ){
               /* NO RDS at all */
               /* We switch the font and layout */
                ui->radiodisplay_multiline->setText(FreqString);
                QFont Fnt = ui->radiodisplay_multiline->font();
                Fnt.setPointSize(36);
                ui->radiodisplay_multiline->setFont(Fnt);
                ui->radiodisplay_multiline->setText(FreqString);
            } else {
                /* We need to decide ig we have the station or if we have more */
                if(RadioText.length()<=0){
                   /* One Line with Text */
                    QFont Fnt = ui->radiodisplay_multiline->font();
                    Fnt.setPointSize(24);
                    ui->radiodisplay_multiline->setFont(Fnt);
                    FreqString = Station+" " +FreqString;
                    ui->radiodisplay_multiline->setText(FreqString);
                } else {
                   /* Two Lines with Text */
                    QFont Fnt = ui->radiodisplay_multiline->font();
                    Fnt.setPointSize(18);
                    ui->radiodisplay_multiline->setFont(Fnt);
                    QString DisplayStr = Station + " " + FreqString +"\n\r"+RadioText;
                    ui->radiodisplay_multiline->setText(DisplayStr);

                }


            }

            if(radio!=nullptr){
                if(radio->rds_avail()==true){
                     ui->RDS->setText("RDS");
                } else {
                   ui->RDS->setText("");
                }
            } else {
                ui->RDS->setText("");
            }
    } else {
        QFont Fnt = ui->radiodisplay_multiline->font();
        Fnt.setPointSize(36);
        ui->radiodisplay_multiline->setFont(Fnt);
        ui->radiodisplay_multiline->setText("Seek");
        ui->RDS->setText("");
    }
    RadioFrequency = frequency;

    /* Last is to get the current volume */
    volume = audiodev->getVolume();
    ui->VolumeLevel->setValue(volume);
    server->SendNewVolume(volume);

}


void MainWindow::on_VOLUME_UP_clicked()
{
    /* increase volume */
    ui->VolumeLevel->setValue(audiodev->IncreaseVolume());
}

void MainWindow::on_VOLUME_DOWN_clicked()
{
    /* decrease volume */
    ui->VolumeLevel->setValue(audiodev->DecreaseVolume());
}

void MainWindow::on_POWER_DOWN_BTN_clicked()
{
   /* We don't use it any more as we now decide for shutdown or quit to desktop */
}


void MainWindow::RemoteInputTimerElapsed(){
    /* Change the station */
    if( (RemoteLastDigit>=1) && ( RemoteLastDigit < 9 ) ){
          RemoteLastDigit--;
          LoadStationMemmory( RemoteLastDigit );
    }
    update_selected_stationpage(0);
    RemoteLastDigit=-1;
}
void MainWindow::processStationkey( uint8_t num ){
    uint8_t station=0;
    if(num <=9 ){

        if(RemoteStationInputTimer->isActive()==true){
            if(RemoteLastDigit >= 0){
                /* We got a second one and can stop the timer */
                RemoteStationInputTimer->stop();
                num--;
                /* And buid the station memory to access */
                if(RemoteLastDigit == 0 ){
                    /* We use the current page + offset */
                     station = ( CurrentStationPage * STATIONPERPAGE ) + num;
                      LoadStationMemmory(station);
                } else {
                    if(RemoteLastDigit>0){
                        RemoteLastDigit--;
                    }
                    station = ( RemoteLastDigit * STATIONPERPAGE ) + num;
                    if(RemoteLastDigit>=ConfiguredStationPages){
                        /* Out of bound */
                    } else {
                        update_selected_stationpage(RemoteLastDigit);
                        LoadStationMemmory(station);
                    }
                }


                /* Cleanup */
                RemoteLastDigit=-1;

            } else {
                // This shall not happen, we do a restart
                RemoteStationInputTimer->stop();
                RemoteLastDigit=num;
                RemoteStationInputTimer->start();
            }

        } else {
            if(RemoteLastDigit >= 0){
                // This may happen if a cleanup failed
                 RemoteLastDigit=num;
                 RemoteStationInputTimer->start();
            } else {
                RemoteLastDigit = num ;
                RemoteStationInputTimer->start();
            }

        }
    }
}


void MainWindow::on_keyPressEvent(QKeyEvent *event){

    event->accept();
}

void MainWindow::on_keyReleaseEvent(QKeyEvent *event){

    if(frqeditwdn->isVisible()==false){
    switch(event->key() ){

        case Qt::Key_Left:{
            QFont Fnt = ui->radiodisplay_multiline->font();
            Fnt.setPointSize(36);
            ui->radiodisplay_multiline->setFont(Fnt);
            ui->radiodisplay_multiline->setText("Seek");
            radio->Seek(false,true);
        } break;

        case Qt::Key_Right:{
            QFont Fnt = ui->radiodisplay_multiline->font();
            Fnt.setPointSize(36);
            ui->radiodisplay_multiline->setFont(Fnt);
            ui->radiodisplay_multiline->setText("Seek");
            radio->Seek(true,true);
        } break;

        case Qt::Key_Plus:{
            on_VOLUME_UP_clicked();
        } break;

        case Qt::Key_Minus:{
            on_VOLUME_DOWN_clicked();
        } break;

        case Qt::Key_Down:{
         this->on_FREQ_DOWN_pressed();
         this->on_FREQ_DOWN_released();
        } break;

        case Qt::Key_Up:{
          this->on_FREQ_UP_pressed();
          this->on_FREQ_UP_released();
        } break;

        case Qt::Key_E:{
            frqeditwdn->update_frequnecy( RadioFrequency );
            frqeditwdn->showFullScreen();
        } break;

        case Qt::Key_Q:{
               this->SaveConfig();
               this->Shutdown(false);
               QProcess exec;
               exec.start("sh",QStringList() << "-c" << "sudo shutdown now");
               exec.waitForFinished();

        } break;

        case Qt::Key_M:{
            on_VOLUME_MUTE_clicked();
        } break;

        /* For the keys 1-8 it becomes tricky */
        /* We will use a timer to have a page access */
        case Qt::Key_1:{
            processStationkey(1);
        } break;

        case Qt::Key_2:{
            processStationkey(2);
        } break;

        case Qt::Key_3:{
            processStationkey(3);
        } break;

        case Qt::Key_4:{
            processStationkey(4);
        } break;

        case Qt::Key_5:{
            processStationkey(5);
        } break;

        case Qt::Key_6:{
            processStationkey(6);
        } break;

        case Qt::Key_7:{
            processStationkey(7);
        } break;

        case Qt::Key_8:{
            processStationkey(8);
        } break;

        /* 0 and 9 are not used here for station access but for page access e.g. 01 and 99*/
        case Qt::Key_0:{
            processStationkey(0);
        }break;

        case Qt::Key_9:{
            processStationkey(9);
        }break;


        default:{
            fprintf(stderr, "Got Key: %i",event->key());
        } break;

    }

     event->accept();
    }
}

void MainWindow::SaveConfig( void ){
    QJsonObject recordObject;
    QJsonArray Stationarray;
    QJsonObject Station[STATIONPERPAGE * ConfiguredStationPages];
    for(uint32_t i=0;i<  STATIONPERPAGE * ConfiguredStationPages ;i++){
        Station[i].insert ("Used" , QJsonValue((bool)StationMemory[i].used));
        Station[i].insert ("Frequency" , QJsonValue((int)StationMemory[i].frequency));
        if(StationMemory[i].StationName != nullptr){
            Station[i].insert ("Name" , QJsonValue(*StationMemory[i].StationName));
        } else {
            Station[i].insert ("Name" , QJsonValue(""));
        }
        Stationarray.insert(i,Station[i]);
    }

    recordObject.insert("LastFrequency", QJsonValue((int)RadioFrequency));
    recordObject.insert("Volume", QJsonValue((int)audiodev->getVolume()));
    recordObject.insert("Mute",QJsonValue((bool)audiodev->getMute()));
    recordObject.insert("StationMemmory",QJsonArray(Stationarray));
    recordObject.insert("StationPages",QJsonValue((int)ConfiguredStationPages));
    QJsonDocument doc(recordObject);

    QFile file("config.json");
    if (true == file.open(QIODevice::WriteOnly | QIODevice::Text )){
        QTextStream out(&file);
        out << doc.toJson();
        file.close();

    } else {

    }
}

void MainWindow::Shutdown(bool appexit){
    SaveConfig();
    AutoscrollingTimer->stop();
    DisplayUpdateTimer->stop();

    StoreStationTimer->stop();
    ExitToDesktopTimer->stop();

    delete AutoscrollingTimer;
    delete DisplayUpdateTimer;

    delete StoreStationTimer;
    delete ExitToDesktopTimer;


    frqeditwdn->close();
    delete frqeditwdn;

    delete  evf;
    delete StationMemory;




    /* We stop all timers and clean up */
    if(radio!=nullptr){
         fprintf(stderr, "Close Tuner");
         radio->TunerClose();
         fprintf(stderr, "Tuner Closed");
         delete radio;
    }

    if(audiodev != nullptr ){
         fprintf(stderr, "Close Audio");
        audiodev->Shutdown();
        fprintf(stderr, "Audio Closed");
        delete audiodev;
    }

    if(appexit == true){
        QApplication::exit(0);
    }
}

void MainWindow::on_VOLUME_MUTE_clicked()
{
    /* Mute unmute of audio */
    /* We do this at tuner level ? */
    if(audiodev->toggleMute()== true){
        /* Change ICON */
        ui->VOLUME_MUTE->setIcon(QIcon(":/icons/volume-mute.png"));
    } else {
        ui->VOLUME_MUTE->setIcon(QIcon(":/icons/volume.png"));
    }

}


void MainWindow::StoreStation(){
    QString FreqString = QString::number(((double)RadioFrequency/1000000),'f',2)+" MHz";

    QString Station;
    radio->GetRdsStation(&Station);
    Station = Station.trimmed();
    if(Station.length()>0){
        FreqString=FreqString+"\n\r"+Station;
    }
    ui->SaveIcon->setIcon(QIcon(":/icons/bookmark-new-2.png"));
    ui->SaveIcon->setVisible(true);
    switch(StoreButtonId){
        case 0:{
            ui->M1->setText(FreqString);
        } break;

        case 1:{
            ui->M2->setText(FreqString);
        } break;

        case 2:{
            ui->M3->setText(FreqString);
        } break;

        case 3:{
            ui->M4->setText(FreqString);
        } break;

        case 4:{
            ui->M5->setText(FreqString);
        } break;

        case 5:{
            ui->M6->setText(FreqString);
        } break;

        case 6:{
            ui->M7->setText(FreqString);
        } break;

        case 7:{
            ui->M8->setText(FreqString);
        } break;

    }
    if(StoreButtonId < 8){
        StationMemory[StoreButtonId+(CurrentStationPage*STATIONPERPAGE)].frequency = RadioFrequency;
        *StationMemory[StoreButtonId+(CurrentStationPage*STATIONPERPAGE)].StationName = FreqString;
        StationMemory[StoreButtonId+(CurrentStationPage*STATIONPERPAGE)].used = true;
    }

}

void MainWindow::LoadStationMemmory(uint32_t idx){
    if( (radio!=nullptr) && ( idx < STATIONPERPAGE * ConfiguredStationPages ) ){
        if( (true == StationMemory[StoreButtonId].used) &&
            ( StationMemory[StoreButtonId].frequency >=89000000 ) &&
            (StationMemory[StoreButtonId].frequency <=108000000) ) {
               /* only do a change if the frequency is off */
            if(radio->getfrequency()!=StationMemory[idx].frequency){
               radio->changefrequency(StationMemory[idx].frequency );
            }
        }
    }
}

void MainWindow::on_M1_pressed()
{
    /*Start Timer for M1*/
    StoreStationTimer->stop();
    StoreButtonId =0;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);



    StoreStationTimer->start();
}

void MainWindow::on_M1_released()
{
     /* What ever is stored now in the button we will display it */
       StoreStationTimer->stop();
        ui->SaveIcon->setVisible(false);
       LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}

void MainWindow::on_M2_pressed()
{


    StoreStationTimer->stop();
    StoreButtonId =1;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}


void MainWindow::on_M2_released()
{
 StoreStationTimer->stop();
 ui->SaveIcon->setVisible(false);
 LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}

void MainWindow::on_M3_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =2;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M3_released()
{
 StoreStationTimer->stop();
 ui->SaveIcon->setVisible(false);
 LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}

void MainWindow::on_M4_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =3;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M4_released()
{

    StoreStationTimer->stop();
    ui->SaveIcon->setVisible(false);
    LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}

void MainWindow::on_M5_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =4;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M5_released()
{
     StoreStationTimer->stop();
     ui->SaveIcon->setVisible(false);
     LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));

}

void MainWindow::on_M6_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =5;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M6_released()
{
 StoreStationTimer->stop();
 ui->SaveIcon->setVisible(false);
 LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));

}

void MainWindow::on_M7_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =6;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M7_released()
{
 StoreStationTimer->stop();
 ui->SaveIcon->setVisible(false);
 LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}

void MainWindow::on_M8_pressed()
{

    StoreStationTimer->stop();
    StoreButtonId =7;
    StoreStationTimer->setSingleShot(true);
    StoreStationTimer->setInterval(3000);
    StoreStationTimer->start();
}

void MainWindow::on_M8_released()
{
 StoreStationTimer->stop();
 ui->SaveIcon->setVisible(false);
 LoadStationMemmory(StoreButtonId+(CurrentStationPage*STATIONPERPAGE));
}



void MainWindow::on_radiodisplay_multiline_clicked()
{
    frqeditwdn->update_frequnecy( RadioFrequency );
    frqeditwdn->showFullScreen();
}

void MainWindow::on_POWER_DOWN_BTN_pressed()
{
    TimeSpan.start();
    ExitToDesktopTimer->stop();
    WillExitToDesktop=false;
    ExitToDesktopTimer->setSingleShot(true);
    ExitToDesktopTimer->start(3000);


}

void MainWindow::on_POWER_DOWN_BTN_released()
{
    ui->SaveIcon->setVisible(false);
    if(WillExitToDesktop==true) {
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, "piRadio",
                                                                    tr("Really back to the Desktop?\n"),
                                                                    QMessageBox::No | QMessageBox::Yes,
                                                                        QMessageBox::Yes);
            if (resBtn != QMessageBox::Yes) {
                SaveConfig();
                return;
            } else {
                  SaveConfig();
                  this->Shutdown(true);
            }
        } else {

                SaveConfig();
                this->Shutdown(false);
                QProcess exec;
                exec.start("sh",QStringList() << "-c" << "sudo shutdown now");
                exec.waitForFinished();
         }



}

void MainWindow::ShowExitToDesktop( void ){
    ui->SaveIcon->setIcon(QIcon(":/icons/user-desktop-2.png"));
    ui->SaveIcon->setVisible(true);
    WillExitToDesktop=true;

}

void MainWindow::closeEvent (QCloseEvent *event)
{
        event->accept();
}

void MainWindow::on_pushButton_clicked()
{
    /* This will show the about box */
    QMessageBox::information(this,"About","Icon  : Typicons byStephen Hutchings, \n\r"
                                          "URL   : 'http://s-ings.com/typicons/ \n\r'"
                                          "Icons : 'Silk icon set 1.3' by Mark James \n\r'"
                                          "URL   : 'http://www.famfamfam.com/lab/icons/silk/' \n\r"
                                          "Build using QT LGPL & GPL Version ", QMessageBox::Ok,QMessageBox::Ok);

}

void MainWindow::update_selected_stationpage( uint8_t page){
    /* Also we use on of our 5 color / styles for the button */
    if( ( (uint16_t)(page+1) > ConfiguredStationPages) || (page == CurrentStationPage) ){
        /* should not happen */

    } else {
    CurrentStationPage = page;
    switch(CurrentStationPage%5){
        case 0:{
            ui->M4_2->setStyleSheet("color:white;");
        } break;

        case 1:{
            ui->M4_2->setStyleSheet("color:white;\nbackground-color: rgb(196, 160, 0);");
        }break;

        case 2:{
           ui->M4_2->setStyleSheet("color:white;\nbackground-color: rgb(206, 92, 0);");
        } break;

        case 3:{
           ui->M4_2->setStyleSheet("color:black;\nbackground-color: rgb(211, 215, 207);");
        } break;

        case 4:{
            ui->M4_2->setStyleSheet("color:white;\nbackground-color: rgb(32, 74, 135);");
        } break;

        default:{
            ui->M4_2->setStyleSheet("color:white;");
        };
    }



    ui->M4_2->setText(QString("M%1").arg(CurrentStationPage+1));
    /* Reload Buttons */
    ReloadButtons();
   }
}

void MainWindow::on_M4_2_clicked()
{
    /* We need to be sure none of the station buttons is clicked */
    /* We change the current page to a new one and need to reload the buttons */
    if( CurrentStationPage+1 >= ConfiguredStationPages){
         update_selected_stationpage(0);
    } else {
        update_selected_stationpage(CurrentStationPage+1);
    }


}

void MainWindow::ReloadButtons( void ){
    /* Next is to populate the icons with text */
    for(uint32_t i=0;i<STATIONPERPAGE;i++){
        if( StationMemory[CurrentStationPage*STATIONPERPAGE+i].used == true ){
          StationMemoryButtonArray[i]->setText(*StationMemory[CurrentStationPage*STATIONPERPAGE+i].StationName);
        } else {
          StationMemoryButtonArray[i]->setText("Not used");
        }
    }
}
