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

#include <QObject>
#include <QtTest>
#include <QDebug>
#include <QtContacts>

#include "config.h"
#include "common/vcard-parser.h"

using namespace QtContacts;

class ContactTimeStampTest : public QObject
{
    Q_OBJECT
private:
    QContactManager *m_manager;

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
        // wait for address-book-service
        QTest::qWait(1000);
    }

    void init()
    {
        m_manager = new QContactManager("galera");
    }

    void cleanup()
    {
        delete m_manager;
    }

    void testFilterContactByChangeLog()
    {
        QDateTime currentDate = QDateTime::currentDateTime();
        // wait one sec to cause a create date later
        QTest::qWait(1000);
        QContact contact = galera::VCardParser::vcardToContact(QStringLiteral("BEGIN:VCARD\r\n"
                                                                              "VERSION:3.0\r\n"
                                                                              "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                              "N:;Fulano;;;\r\n"
                                                                              "EMAIL:fulano@gmail.com\r\n"
                                                                              "TEL:123456\r\n"
                                                                              "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                              "END:VCARD\r\n"));

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // qery contacts;
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QContactTimestamp timestamp = contacts[0].detail<QContactTimestamp>();
        qDebug() << "CURRENT DATE:" << currentDate.toUTC() << "\n"
                 << "CREATED DATE:" << timestamp.created() << "\n"
                 << "MODIFIED DATE:" << timestamp.lastModified();

        QVERIFY(timestamp.created().isValid());
        QVERIFY(timestamp.lastModified().isValid());
        QVERIFY(timestamp.created() >= currentDate);
        QVERIFY(timestamp.lastModified() >= currentDate);

        QContactChangeLogFilter changeLogFilter;
        changeLogFilter.setSince(currentDate);

        QList<QContactId> ids = m_manager->contactIds(changeLogFilter);
        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0], contacts[0].id());
    }
};

QTEST_MAIN(ContactTimeStampTest)

#include "contact-timestamp-test.moc"
