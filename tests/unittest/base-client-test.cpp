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
#include "lib/qindividual.h"
#include "lib/source.h"

#include <QtCore/QDebug>
#include <QtTest/QTest>

void BaseClientTest::initTestCase()
{
    galera::Source::registerMetaType();
    m_serverIface = new QDBusInterface(CPIM_SERVICE_NAME,
                                       CPIM_ADDRESSBOOK_OBJECT_PATH,
                                       CPIM_ADDRESSBOOK_IFACE_NAME);
    QVERIFY(!m_serverIface->lastError().isValid());

    m_dummyIface = new QDBusInterface(DUMMY_SERVICE_NAME,
                                      DUMMY_OBJECT_PATH,
                                      DUMMY_IFACE_NAME);
    QVERIFY(!m_dummyIface->lastError().isValid());
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
