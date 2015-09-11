/*
 * Copyright 2015 Canonical Ltd.
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

#include <QDebug>
#include <QtContacts>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusConnection>

#include "config.h"
#include "common/dbus-service-defs.h"

using namespace QtContacts;

class BaseEDSTest
{
protected:
    QContactManager *m_manager;

    bool isReady()
    {
        QDBusMessage callIsReady = QDBusMessage::createMethodCall(CPIM_SERVICE_NAME,
                                                                  CPIM_ADDRESSBOOK_OBJECT_PATH,
                                                                  CPIM_ADDRESSBOOK_IFACE_NAME,
                                                                  "isReady");
         QDBusReply<bool> reply = QDBusConnection::sessionBus().call(callIsReady);
         return reply.value();
    }

    bool shutDownServer()
    {
        QDBusMessage callShutDown = QDBusMessage::createMethodCall(CPIM_SERVICE_NAME,
                                                                  CPIM_ADDRESSBOOK_OBJECT_PATH,
                                                                  CPIM_ADDRESSBOOK_IFACE_NAME,
                                                                  "shutDown");
        QDBusConnection::sessionBus().call(callShutDown);
    }

    void initTestCaseImpl()
    {
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
        qRegisterMetaType<QList<QtContacts::QContactId> >("QList<QContactId>");
    }

    void cleanupTestCaseImpl()
    {
        shutDownServer();
    }

    void initImpl()
    {
        m_manager = new QContactManager("galera");
        QTRY_VERIFY_WITH_TIMEOUT(isReady(), 60000);
    }

    void cleanupImpl()
    {
        delete m_manager;
    }
};

