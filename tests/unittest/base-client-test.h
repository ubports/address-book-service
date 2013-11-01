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

#ifndef __BASE_DUMMY_TEST_H__
#define __BASE_DUMMY_TEST_H__

#include <QtCore/QObject>
#include <QtContacts/QContact>
#include <QtDBus/QDBusInterface>

class BaseClientTest : public QObject
{
    Q_OBJECT
protected:
    QDBusInterface *m_serverIface;
    QDBusInterface *m_dummyIface;

protected Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
};

#endif
