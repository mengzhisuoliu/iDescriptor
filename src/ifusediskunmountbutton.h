#ifndef IFUSEDISKUNMOUNTBUTTON_H
#define IFUSEDISKUNMOUNTBUTTON_H

#include "iDescriptor-ui.h"

class iFuseDiskUnmountButton : public ZIconWidget
{
    Q_OBJECT
public:
    explicit iFuseDiskUnmountButton(const QString &path,
                                    QWidget *parent = nullptr);

signals:
};

#endif // IFUSEDISKUNMOUNTBUTTON_H
