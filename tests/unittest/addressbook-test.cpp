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
#include "lib/source.h"
#include "common/dbus-service-defs.h"

#include <QObject>
#include <QtDBus>
#include <QtTest>
#include <QDebug>

class AddressBookTest : public BaseClientTest
{
    Q_OBJECT
private:
    QEventLoop *m_eventLoop;

private Q_SLOTS:

    void testSortFields()
    {
        QStringList defaultSortFields;
        defaultSortFields << "ADDR_COUNTRY"
                          << "ADDR_LOCALITY"
                          << "ADDR_POSTCODE"
                          << "ADDR_POST_OFFICE_BOX"
                          << "ADDR_REGION"
                          << "ADDR_STREET"
                          << "BIRTHDAY"
                          << "EMAIL"
                          << "FIRST_NAME"
                          << "FULL_NAME"
                          << "IM_PROTOCOL"
                          << "IM_URI"
                          << "LAST_NAME"
                          << "MIDLE_NAME"
                          << "NAME_PREFIX"
                          << "NAME_SUFFIX"
                          << "NICKNAME"
                          << "ORG_DEPARTMENT"
                          << "ORG_LOCATION"
                          << "ORG_NAME"
                          << "ORG_ROLE"
                          << "ORG_TITLE"
                          << "PHONE"
                          << "PHOTO"
                          << "URL";
        QDBusReply<QStringList> reply = m_serverIface->call("sortFields");
        QCOMPARE(reply.value(), defaultSortFields);
    }

    void testSource()
    {
        QDBusReply<galera::Source> reply = m_serverIface->call("source");
        QVERIFY(reply.isValid());
        QCOMPARE(reply.value().id(), QStringLiteral("dummy-store"));
    }

    void testAvailableSources()
    {
        QDBusReply<QList<galera::Source> > reply = m_serverIface->call("availableSources");
        galera::SourceList list = reply.value();
        QCOMPARE(list.count(), 1);
        galera::Source src = list[0];
        QCOMPARE(src.id(), QStringLiteral("dummy-store"));
    }
};

QTEST_MAIN(AddressBookTest)

#include "addressbook-test.moc"
