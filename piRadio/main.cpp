#include "mainwindow.h"
#include <QSplashScreen>
#include <QApplication>

int main(int argc, char *argv[])
{
    qRegisterMetaType < uint32_t >("uint32_t");
    qRegisterMetaType < uint8_t >("uint8_t");
    qRegisterMetaType < uint16_t >("uint16_t");
    qRegisterMetaType < bool >("bool");
    QApplication a(argc, argv);
    MainWindow w;
    w.showFullScreen();
    w.startRadio();
    return a.exec();
}

