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
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusConnection>

#include "config.h"
#include "common/vcard-parser.h"
#include "common/dbus-service-defs.h"

using namespace QtContacts;

class ContactAvatarTest : public QObject
{
    Q_OBJECT
private:
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

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setLibraryPaths(QStringList() << QT_PLUGINS_BINARY_DIR);
        qRegisterMetaType<QList<QtContacts::QContactId> >("QList<QContactId>");
        // wait for address-book-service
        QTest::qWait(1000);
    }

    void init()
    {
        m_manager = new QContactManager("galera");
        // wait to be ready
        QTRY_VERIFY_WITH_TIMEOUT(isReady(), 60000);
    }

    void cleanup()
    {
        QList<QContactId> contacts = m_manager->contactIds();
        m_manager->removeContacts(contacts);
        delete m_manager;
    }

    void testCreateAvatarWithRevision()
    {
        QContact contact = galera::VCardParser::vcardToContact(QString("BEGIN:VCARD\r\n"
                                                                       "VERSION:3.0\r\n"
                                                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                       "N:;Fulano;;;\r\n"
                                                                       "EMAIL:fulano@gmail.com\r\n"
                                                                       "TEL:123456\r\n"
                                                                       "X-AVATAR-REV:1234567890\r\n"
                                                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                       "PHOTO;VALUE=URL:file://%1/avatar.svg\r\n"
                                                                       "END:VCARD\r\n").arg(TEST_DATA_DIR));

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // qery contacts;
        QContactFilter filter;
        QList<QContact> contacts = m_manager->contacts(filter);
        QCOMPARE(contacts.size(), 1);

        QContactExtendedDetail avatarRev;
        Q_FOREACH(const QContactExtendedDetail &det, contacts[0].details<QContactExtendedDetail>()) {
            if (det.name() == "X-AVATAR-REV") {
                avatarRev = det;
            }
        }
        QCOMPARE(avatarRev.data().toString(), QStringLiteral("1234567890"));

        QContactAvatar avatar = contacts[0].detail<QContactAvatar>();
        QVERIFY(avatar.imageUrl().toString().startsWith(QStringLiteral("file:///tmp/contact-avatar-test-")));
    }

    void testModifyAvatarLocally()
    {
        QContact contact = galera::VCardParser::vcardToContact(QString("BEGIN:VCARD\r\n"
                                                                       "VERSION:3.0\r\n"
                                                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                       "N:;Fulano;;;\r\n"
                                                                       "EMAIL:fulano@gmail.com\r\n"
                                                                       "TEL:123456\r\n"
                                                                       "X-AVATAR-REV:1234567890\r\n"
                                                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                       "PHOTO;VALUE=URL:file://%1/avatar.svg\r\n"
                                                                       "END:VCARD\r\n").arg(TEST_DATA_DIR));

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // qery contacts;
        QList<QContact> contacts = m_manager->contacts();
        QCOMPARE(contacts.size(), 1);

        // update a contact
        QContact newContact(contacts[0]);
        QContactAvatar avatar = newContact.detail<QContactAvatar>();
        QUrl oldAvatar = avatar.imageUrl();
        avatar.setImageUrl(QUrl::fromLocalFile(QString("%1/new_avatar.svg").arg(TEST_DATA_DIR)));
        newContact.saveDetail(&avatar);

        QSignalSpy spyContactChanged(m_manager, SIGNAL(contactsChanged(QList<QContactId>)));
        result = m_manager->saveContact(&newContact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactChanged.count(), 1);

        // query the updated contact
        contacts = m_manager->contacts();
        QCOMPARE(contacts.size(), 1);
        newContact = contacts[0];

        // check if contact was updated
        QContactExtendedDetail avatarRev;
        Q_FOREACH(const QContactExtendedDetail &det, newContact.details<QContactExtendedDetail>()) {
            if (det.name() == "X-AVATAR-REV") {
                avatarRev = det;
            }
        }
        // Avatar revision must be reseted during a loca update
        QVERIFY(avatarRev.data().toString().isEmpty());

        // Avatar should have a new value
        avatar = newContact.detail<QContactAvatar>();
        QVERIFY(!avatar.imageUrl().isEmpty());
        QVERIFY(avatar.imageUrl() != oldAvatar);
    }

    void testModifyAvatarAndVersion()
    {
        QContact contact = galera::VCardParser::vcardToContact(QString("BEGIN:VCARD\r\n"
                                                                       "VERSION:3.0\r\n"
                                                                       "X-REMOTE-ID:1dd7d51a1518626a\r\n"
                                                                       "N:;Fulano;;;\r\n"
                                                                       "EMAIL:fulano@gmail.com\r\n"
                                                                       "TEL:123456\r\n"
                                                                       "X-AVATAR-REV:1234567890\r\n"
                                                                       "CLIENTPIDMAP:56183a5b-5da7-49fe-8cf6-9bfd3633bf6d\r\n"
                                                                       "PHOTO;VALUE=URL:file://%1/avatar.svg\r\n"
                                                                       "END:VCARD\r\n").arg(TEST_DATA_DIR));

        // create a contact
        QSignalSpy spyContactAdded(m_manager, SIGNAL(contactsAdded(QList<QContactId>)));
        bool result = m_manager->saveContact(&contact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactAdded.count(), 1);

        // qery contacts;
        QList<QContact> contacts = m_manager->contacts();
        QCOMPARE(contacts.size(), 1);

        // update a contact
        QContact newContact(contacts[0]);
        QContactAvatar avatar = newContact.detail<QContactAvatar>();
        QUrl oldAvatar = avatar.imageUrl();
        avatar.setImageUrl(QUrl::fromLocalFile(QString("%1/new_avatar.svg").arg(TEST_DATA_DIR)));
        newContact.saveDetail(&avatar);

        // udate avatar version
        QContactExtendedDetail avatarRev;
        Q_FOREACH(const QContactExtendedDetail &det, newContact.details<QContactExtendedDetail>()) {
            if (det.name() == "X-AVATAR-REV") {
                avatarRev = det;
            }
        }
        QString avatarRevValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        avatarRev.setData(avatarRevValue);
        newContact.saveDetail(&avatarRev);

        // save contact
        QSignalSpy spyContactChanged(m_manager, SIGNAL(contactsChanged(QList<QContactId>)));
        result = m_manager->saveContact(&newContact);
        QCOMPARE(result, true);
        QTRY_COMPARE(spyContactChanged.count(), 1);

        // query the updated contact
        contacts = m_manager->contacts();
        QCOMPARE(contacts.size(), 1);
        newContact = contacts[0];

        // check if contact was updated
        avatarRev = QContactExtendedDetail();
        Q_FOREACH(const QContactExtendedDetail &det, newContact.details<QContactExtendedDetail>()) {
            if (det.name() == "X-AVATAR-REV") {
                avatarRev = det;
            }
        }
        // Avatar revision must be updated to the new value
        QCOMPARE(avatarRev.data().toString(), avatarRevValue);

        // Avatar should have a new value
        avatar = newContact.detail<QContactAvatar>();
        QVERIFY(!avatar.imageUrl().isEmpty());
        QVERIFY(avatar.imageUrl() != oldAvatar);
    }
};

QTEST_MAIN(ContactAvatarTest)

#include "contact-avatar-test.moc"
