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

