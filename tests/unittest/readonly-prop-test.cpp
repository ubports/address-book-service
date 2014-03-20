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
#include "common/vcard-parser.h"

#include <QObject>
#include <QtDBus>
#include <QtTest>
#include <QDebug>
#include <QtVersit>

class ReadOnlyPropTest : public BaseClientTest
{
    Q_OBJECT
private:
    QString m_basicVcard;
    QString m_resultBasicVcard;

private Q_SLOTS:
    void initTestCase()
    {
        BaseClientTest::initTestCase();
        m_basicVcard = QStringLiteral("BEGIN:VCARD\n"
                                      "VERSION:3.0\n"
                                      "N:Tal;Fulano_;de;;\n"
                                      "EMAIL:fulano_@ubuntu.com\n"
                                      "TEL;PID=1.1;TYPE=ISDN:33331410\n"
                                      "TEL;PID=1.2;TYPE=CELL:8888888\n"
                                      "URL:www.canonical.com\n"
                                      "END:VCARD");
    }

    void testCreateContactAndReturnReadOlyFileds()
    {
        // call create contact
        QDBusReply<QString> reply = m_serverIface->call("createContact", m_basicVcard, "dummy-store");

        // check if the returned id is valid
        QString newContactId = reply.value();
        QVERIFY(!newContactId.isEmpty());

        // check if the cotact was created with URL as read-only field
        // URL is not a writable property on our dummy backend
        QDBusReply<QStringList> reply2 = m_dummyIface->call("listContacts");
        QCOMPARE(reply2.value().count(), 1);

        QList<QtContacts::QContact> contactsCreated = galera::VCardParser::vcardToContactSync(reply2.value());
        QCOMPARE(contactsCreated.count(), 1);
        Q_FOREACH(QContactDetail det, contactsCreated[0].details()) {
            if (det.type() == QContactDetail::TypeUrl) {
                QVERIFY(det.accessConstraints().testFlag(QContactDetail::ReadOnly));
            } else {
                QVERIFY(!det.accessConstraints().testFlag(QContactDetail::ReadOnly));
            }
        }
    }
};

QTEST_MAIN(ReadOnlyPropTest)

#include "readonly-prop-test.moc"
