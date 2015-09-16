/*
 * Copyright 2015 Canonical Ltd.
 *
 * This file is part of address-book-service.
 *
 * sync-monitor is free software; you can redistribute it and/or modify
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

#include <QtCore/QCoreApplication>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtDBus/QDBusConnection>

#include "ab-update-adaptor.h"
#include "ab-update.h"
#include "dbus-service-defs.h"
#include "config.h"

namespace C {
#include <libintl.h>
}

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName(SETTINGS_ORG);
    QCoreApplication::setApplicationName("AddressBookUpdate");

    setlocale(LC_ALL, "");
    C::bindtextdomain(GETTEXT_PACKAGE, GETTEXT_LOCALEDIR);
    C::bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    QCoreApplication app(argc, argv);

    // AddressBook updater
    QScopedPointer<ABUpdate> abUpdate(new ABUpdate);
    QScopedPointer<ABUpdateAdaptor> abUpdateAdaptor(new ABUpdateAdaptor(abUpdate.data()));


    // connect to D-Bus and register as an object:
    QDBusConnection connection = QDBusConnection::sessionBus();
    bool ret = connection.registerService(CPIM_UPDATE_SERVICE_NAME);
    if (!ret) {
        qWarning() << "Fail to register service:" << CPIM_UPDATE_SERVICE_NAME;
        return -1;
    }

    ret = connection.registerObject(CPIM_UPDATE_OBJECT_PATH, abUpdate.data());
    if (!ret) {
        qWarning() << "Fail to register object:" << CPIM_UPDATE_SERVICE_NAME;
        return -2;
    }

    if (app.arguments().contains("--sync")) {
        QTimer::singleShot(0, abUpdate.data(), SLOT(startUpdate()));
    }
    QObject::connect(abUpdate.data(), SIGNAL(updateDone()),
                     &app, SLOT(quit()));

    qDebug() << "Updater started" << QDateTime::currentDateTime();
    app.exec();
    qDebug() << "Updater finished" << QDateTime::currentDateTime();

    connection.unregisterObject(CPIM_UPDATE_OBJECT_PATH);
    connection.unregisterService(CPIM_UPDATE_SERVICE_NAME);
}
