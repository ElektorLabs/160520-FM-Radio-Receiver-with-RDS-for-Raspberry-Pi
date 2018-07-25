#include "eventfilterforkeys.h"



bool eventFilterForKeys::eventFilter(QObject* obj, QEvent* event)
{
    if ( (event->type()==QEvent::KeyPress) || ( event->type()==QEvent::KeyRelease) ){
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        /* We need to build a singal to trab the keys somewhere else */
            if (event->type()==QEvent::KeyPress){
                emit onKeyPress( key);
            } else {
                emit onKeyRelease( key );
            }

        return true;
    } else {
        return QObject::eventFilter(obj, event);
    }
    return false;
}
