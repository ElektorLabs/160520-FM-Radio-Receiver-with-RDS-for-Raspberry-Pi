#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool startRadio( void );

private slots:

    void on_FREQ_DOWN_pressed();

    void on_FREQ_DOWN_released();

    void FreqDec( void );
    void FreqInc( void );

    void StoreStation( void );
    void ShowExitToDesktop( void );
    void UpdateRadioFreq(uint32_t Frq);

    void on_FREQ_UP_pressed();

    void on_FREQ_UP_released();

    void updatedisplay( void );

     void on_VOLUME_UP_clicked();

     void on_VOLUME_DOWN_clicked();

     void on_POWER_DOWN_BTN_clicked();

     void on_keyPressEvent(QKeyEvent *event);

     void on_keyReleaseEvent(QKeyEvent *event);

     void on_VOLUME_MUTE_clicked();

     void on_M1_pressed();

     void on_M2_pressed();

     void on_M3_pressed();

     void on_M4_pressed();

     void on_M5_pressed();

     void on_M6_pressed();

     void on_M7_pressed();

     void on_M8_pressed();

     void on_M1_released();

     void on_M2_released();

     void on_M3_released();

     void on_M4_released();

     void on_M5_released();

     void on_M6_released();

     void on_M7_released();

     void on_M8_released();

     void on_radiodisplay_multiline_clicked();

     void on_POWER_DOWN_BTN_pressed();

     void on_POWER_DOWN_BTN_released();

     void closeEvent (QCloseEvent *event);

     void on_pushButton_clicked();

     void on_M4_2_clicked();

     void RemoteInputTimerElapsed( );

private:
    Ui::MainWindow *ui;
    void Shutdown( bool );
    void LoadStationMemmory( uint32_t idx);
    void SaveConfig( void );
    void ReloadButtons( void );

    void processStationkey( uint8_t num );
    void update_selected_stationpage( uint8_t page);




};

#endif // MAINWINDOW_H
