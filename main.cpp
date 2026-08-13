#include <qabstractsocket.h>
#include <qapplication.h>
#include <qicon.h>
#include <qmetatype.h>
#include <qnetworkproxy.h>
#include <qvector.h>
#include "iec104/iec104_class.h"
#include "MainWindow.h"

Q_DECLARE_METATYPE(iec_obj)
Q_DECLARE_METATYPE(QVector<iec_obj>)

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/Iec104Client.png"));
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    qRegisterMetaType<QAbstractSocket::SocketState>();
    qRegisterMetaType<iec_obj>("iec_obj");
    qRegisterMetaType<QVector<iec_obj>>("QVector<iec_obj>");
    MainWindow w;
    w.show();
    return a.exec();
}