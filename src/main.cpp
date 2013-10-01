/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * contact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "addressbook.h"
#include "view.h"
#include "source.h"
#include "common/dbus-service-defs.h"

void contactServiceMessageOutput(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &message)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    Q_UNUSED(message);
    //nothing
}


int main(int argc, char** argv)
{
    galera::AddressBook::init();
    galera::Source::registerMetaType();
    QCoreApplication app(argc, argv);

    // disable debug message if variable not exported
    if (qgetenv("CONTACT_SERVICE_DEBUG").isEmpty()) {
        qInstallMessageHandler(contactServiceMessageOutput);
    }

    // Register service
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (connection.interface()->isServiceRegistered(CPIM_SERVICE_NAME)) {
        return false;
    }

    if (!connection.registerService(CPIM_SERVICE_NAME))
    {
        qWarning() << "Could not register service!" << CPIM_SERVICE_NAME;
    } else {
        qDebug() << "Interface registered:" << CPIM_SERVICE_NAME;
    }

    galera::AddressBook *book = new galera::AddressBook;
    app.connect(book, SIGNAL(stopped()), SLOT(quit()));
    book->registerObject(connection);
    int ret = app.exec();

    //delete book;
    return ret;
}

