#include "addressbook.h"
#include "view.h"
#include "source.h"

#define GALERA_SERVICE_NAME "com.canonical.galera"

int main(int argc, char** argv)
{
    galera::Source::registerMetaType();
    QCoreApplication app(argc, argv);

    // Register service
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (connection.interface()->isServiceRegistered(GALERA_SERVICE_NAME)) {
        return false;
    }

    if (!connection.registerService(GALERA_SERVICE_NAME))
    {
        qWarning() << "Could not register service!" << GALERA_SERVICE_NAME;
    } else {
        qDebug() << "Interface registered:" << GALERA_SERVICE_NAME;
    }

    galera::AddressBook *book = new galera::AddressBook;
    book->registerObject(connection);
    int ret = app.exec();

    //delete book;
    return ret;
}

