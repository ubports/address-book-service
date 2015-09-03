#include <QtCore/QCoreApplication>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>

#include "ab-update-adaptor.h"
#include "ab-update.h"
#include "dbus-service-defs.h"
#include "config.h"

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName("Canonical");
    QCoreApplication::setApplicationName("AddressBookUpdate");
    QCoreApplication app(argc, argv);

    // AddressBook updater
    QScopedPointer<ABUpdate> abUpdate(new ABUpdate);
    QScopedPointer<ABUpdateAdaptor> abUpdateAdaptor(new ABUpdateAdaptor(abUpdate.data()));

    // connect to D-Bus and register as an object:
    QDBusConnection::sessionBus().registerObject(CPIM_UPDATE_OBJECT_PATH, abUpdate.data());

    // TODO: implement support for app args (Example: --wipe)
    QTimer::singleShot(0, abUpdate.data(), SLOT(startUpdate()));

    // quit app when update is done
    QObject::connect(abUpdate.data(), SIGNAL(updateDone()), &app, SLOT(quit()));
    app.exec();
}
