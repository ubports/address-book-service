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

#include "folks-dummy-base-test.h"

#include <QObject>
#include <QtTest>
#include <QDebug>

class ServiceLifeCycleTest : public BaseDummyTest
{
    Q_OBJECT
private Q_SLOTS:

    void testServiceReady()
    {
        QCOMPARE(m_addressBook->isReady(), false);
        startService();
        while(!m_addressBook->isReady()) {
            QCoreApplication::instance()->processEvents();
        }
        QCOMPARE(m_addressBook->isReady(), true);
    }
};

QTEST_MAIN(ServiceLifeCycleTest)

#include "service-life-cycle-test.moc"
