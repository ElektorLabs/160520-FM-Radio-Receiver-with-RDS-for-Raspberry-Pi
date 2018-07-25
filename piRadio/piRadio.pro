#-------------------------------------------------
#
# Project created by QtCreator 2018-04-30T10:02:04
#
#-------------------------------------------------

QT       += core gui
QT       += multimedia
QT       += core websockets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = piRadio
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0



SOURCES += \
        main.cpp \
        mainwindow.cpp \
    radiotuner.cpp \
    audioloop.cpp \
    frequencyedit.cpp \
    rds_decoder.cpp \
    audiohelper.cpp \
    eventfilterforkeys.cpp \
    websocketconnector.cpp

HEADERS += \
        mainwindow.h \
    radiotuner.h \
    audioloop.h \
    frequencyedit.h \
    rds_decoder.h \
    audiohelper.h \
    eventfilterforkeys.h \
    websocketconnector.h

FORMS += \
        mainwindow.ui \
    frequencyedit.ui

target.path = /home/pi

INSTALLS += target


target.path = /home/pi

INSTALLS += target

QMAKE_CXXFLAGS += -std=c++11

unix:!macx: LIBS += -lasound

RESOURCES += \
    icons.qrc
