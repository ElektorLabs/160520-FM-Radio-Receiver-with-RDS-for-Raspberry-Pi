#ifndef FREQUENCYEDIT_H
#define FREQUENCYEDIT_H

#include <QDialog>
#include <QKeyEvent>

namespace Ui {
class frequencyedit;
}

class frequencyedit : public QDialog
{
    Q_OBJECT

public:
    explicit frequencyedit(uint32_t Frequency, QWidget *parent = 0);
    void update_frequnecy(uint32_t frequency);
    ~frequencyedit();

signals:

    void FrequencyChanged(uint32_t NewFewquency);


private slots:
    void on_BTN_0_clicked();

    void on_BTN_1_clicked();

    void on_BTN_2_clicked();

    void on_BTN_3_clicked();

    void on_BTN_4_clicked();

    void on_BTN_5_clicked();

    void on_BTN_6_clicked();

    void on_BTN_7_clicked();

    void on_BTN_9_clicked();

    void on_BTN_OKAY_clicked();

    void on_BTN_ABORT_clicked();

    void keyPressEvent(QKeyEvent *event);

    void keyReleaseEvent(QKeyEvent *event);

    void on_BTN_COLON_clicked();

    void on_BTN_BACK_clicked();

    void on_BTN_8_clicked();

private:
    Ui::frequencyedit *ui;
    uint32_t CurrentFrequency;
    QString FrequencyTxt;
    uint32_t Frequency_Mhz;
    uint32_t Frequency_kHz;
    bool Frequency_Valid( void );

    void process_digit(uint8_t digit);
    void process_seperator( void );


};

#endif // FREQUENCYEDIT_H
