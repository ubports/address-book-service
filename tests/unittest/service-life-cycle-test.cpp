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

#include "base-client-test.h"
#include "common/dbus-service-defs.h"

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtDBus/QDBusReply>
#include <QtTest>

class ServiceLifeCycleTest : public BaseClientTest
{
    Q_OBJECT
private Q_SLOTS:
    void testServiceReady()
    {
        QTRY_COMPARE(m_serverIface->property("isReady").toBool(), true);
        QTRY_COMPARE(m_dummyIface->property("isReady").toBool(), true);
    }

    void testCallServiceFunction()
    {
        QDBusReply<bool> result = m_serverIface->call("ping");
        QCOMPARE(result.value(), true);

        result = m_dummyIface->call("ping");
        QCOMPARE(result.value(), true);
    }


    void testServiceShutdown()
    {
        m_dummyIface->call("quit");
        // wait service quits
        QTest::qSleep(100);
        QDBusReply<bool> result = m_serverIface->call("ping");
        QVERIFY(result.error().isValid());

        result = m_dummyIface->call("ping");
        QVERIFY(result.error().isValid());
    }
};

QTEST_MAIN(ServiceLifeCycleTest)

#include "service-life-cycle-test.moc"
