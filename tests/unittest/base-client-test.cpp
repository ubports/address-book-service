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

#include "config.h"
#include "base-client-test.h"
#include "dummy-backend-defs.h"

#include "common/dbus-service-defs.h"
#include "common/source.h"
#include "lib/qindividual.h"

#include <QtCore/QDebug>
#include <QtTest/QTest>
#include <QtContacts/QContactId>

void BaseClientTest::initTestCase()
{
    QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
    galera::Source::registerMetaType();
    qRegisterMetaType<QList<QtContacts::QContactId> >("QList<QContactId>");

    QString serviceName;
    if (qEnvironmentVariableIsSet(ALTERNATIVE_CPIM_SERVICE_NAME)) {
        serviceName = qgetenv(ALTERNATIVE_CPIM_SERVICE_NAME);
    } else {
        serviceName = CPIM_SERVICE_NAME;
    }

    m_serverIface = new QDBusInterface(serviceName,
                                       CPIM_ADDRESSBOOK_OBJECT_PATH,
                                       CPIM_ADDRESSBOOK_IFACE_NAME);
    QVERIFY(!m_serverIface->lastError().isValid());
    // wait for service to be ready
    QTRY_COMPARE_WITH_TIMEOUT(m_serverIface->property("isReady").toBool(), true, 10000);

    m_dummyIface = new QDBusInterface(DUMMY_SERVICE_NAME,
                                      DUMMY_OBJECT_PATH,
                                      DUMMY_IFACE_NAME);
    QVERIFY(!m_dummyIface->lastError().isValid());
    // wait for service to be ready
    QTRY_COMPARE_WITH_TIMEOUT(m_dummyIface->property("isReady").toBool(), true, 10000);
}

void BaseClientTest::init()
{
}

void BaseClientTest::cleanup()
{
    m_dummyIface->call("reset");
    QDBusReply<QStringList> reply = m_dummyIface->call("listContacts");
    QCOMPARE(reply.value().count(), 0);
}

void BaseClientTest::cleanupTestCase()
{
    m_dummyIface->call("quit");
    delete m_dummyIface;
    delete m_serverIface;
}
