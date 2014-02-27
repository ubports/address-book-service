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
#include "lib/qindividual.h"
#include "common/dbus-service-defs.h"
#include "common/vcard-parser.h"

#include <QObject>
#include <QtDBus>
#include <QtTest>
#include <QDebug>
#include <QtVersit>

class ContactLinkTest : public BaseClientTest
{
    Q_OBJECT
private:
    QString m_vcard;

private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        m_vcard = QStringLiteral("BEGIN:VCARD\n"
                                 "VERSION:3.0\n"
                                 "N:tal;fulano_;de;;\n"
                                 "EMAIL:email@ubuntu.com\n"
                                 "TEL;PID=1.1;TYPE=ISDN:33331410\n"
                                 "END:VCARD");
    }

    void testCreateContactWithSameEmail()
    {
        // call create contact
        QDBusReply<QString> reply = m_serverIface->call("createContact", m_vcard, "dummy-store");

        // check if the returned id is valid
        QString contactAId = reply.value();
        QVERIFY(!contactAId.isEmpty());

        QString vcardB = m_vcard.replace("fulano", "contact");
        reply = m_serverIface->call("createContact", vcardB, "dummy-store");

        // check if the returned id is valid
        QString contactBId = reply.value();
        QVERIFY(!contactBId.isEmpty());

        QVERIFY(contactAId != contactBId);
    }

    void testAppendOtherContactEmail()
    {
        // call create contact
        QDBusReply<QString> reply = m_serverIface->call("createContact", m_vcard, "dummy-store");

        // check if the returned id is valid
        QString contactAId = reply.value();
        QVERIFY(!contactAId.isEmpty());

        // add a contact with diff email
        QString vcardB = m_vcard.replace("fulano", "contact").replace("email","email2");
        reply = m_serverIface->call("createContact", vcardB, "dummy-store");

        // check if the returned id is valid
        QString contactBId = reply.value();
        QVERIFY(!contactBId.isEmpty());

        // udate contactB with same email as contactA
        vcardB = m_vcard.replace("fulano", "contact");
        reply = m_serverIface->call("createContact", vcardB, "dummy-store");

        // check if the returned id is valid
        contactBId = reply.value();
        QVERIFY(!contactBId.isEmpty());

        // check if id still different
        QVERIFY(contactAId != contactBId);
    }
};

QTEST_MAIN(ContactLinkTest)

#include "contact-link-test.moc"
