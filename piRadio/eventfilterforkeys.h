#ifndef EVENTFILTERFORKEYS_H
#define EVENTFILTERFORKEYS_H
#include <QObject>
#include <QEvent>
#include <QKeyEvent>


class eventFilterForKeys:public QObject
{
    Q_OBJECT
protected:
        bool eventFilter(QObject* obj, QEvent* event);

signals:
    void onKeyPress( QKeyEvent *event);
    void onKeyRelease ( QKeyEvent *event);


};

#endif // EVENTFILTERFORKEYS_H
