#include "ifusediskunmountbutton.h"
#include "iDescriptor-ui.h"
#include <QApplication>
#include <QMessageBox>

iFuseDiskUnmountButton::iFuseDiskUnmountButton(const QString &path,
                                               QWidget *parent)
    : ZIconWidget{QIcon(":/resources/icons/ClarityHardDiskSolidAlerted.png"),
                  "Unmount iFuse at " + path, parent}
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(24, 24);
}
