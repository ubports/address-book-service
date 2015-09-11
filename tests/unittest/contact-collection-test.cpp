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

#include "common/dbus-service-defs.h"
#include "config.h"

using namespace QtContacts;

class ContactCollectionTest : public QObject
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
        delete m_manager;
    }

    /*
     * Test create a new collection
     */
    void testCreateAddressBook()
    {
        QContact c;
        c.setType(QContactType::TypeGroup);
        QContactDisplayLabel label;
        QString uniqueName = QString("%1").arg(QUuid::createUuid().toString().remove("{").remove("}"));
        label.setLabel(uniqueName);
        c.saveDetail(&label);

        bool saveResult = m_manager->saveContact(&c);
        QVERIFY(saveResult);

        QContactDetailFilter filter;
        filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        filter.setValue(QContactType::TypeGroup);

        QList<QContact> contacts = m_manager->contacts(filter);
        Q_FOREACH(const QContact &contact, contacts) {
            QVERIFY(c.id().toString().startsWith("qtcontacts:galera::source@"));
            if ((contact.detail<QContactDisplayLabel>().label() == uniqueName) &&
                (contact.id() == c.id())) {
                return;
            }
        }
        QFAIL("New collection not found");
    }

    /*
     * Test remove a collection
     */
    void testRemoveAddressBook()
    {
        // create a source
        QContact c;
        c.setType(QContactType::TypeGroup);
        QContactDisplayLabel label;
        QString uniqueName = QString("%1").arg(QUuid::createUuid().toString().remove("{").remove("}"));
        label.setLabel(uniqueName);
        c.saveDetail(&label);

        bool saveResult = m_manager->saveContact(&c);
        QVERIFY(saveResult);

        // try to remove new source
        bool removeResult = m_manager->removeContact(c.id());
        QVERIFY(removeResult);

        // check if the source was removed
        QContactDetailFilter filter;
        filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        filter.setValue(QContactType::TypeGroup);
        QList<QContact> contacts = m_manager->contacts(filter);
        Q_FOREACH(const QContact &contact, contacts) {
            if (contact.id() == c.id()) {
                QFAIL("Collection not removed");
            }
        }
    }

    /*
     * Test query for collections using the contact group type
     */
    void testQueryAddressBook()
    {
        // filter all contact groups/addressbook
        QContactDetailFilter filter;
        filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        filter.setValue(QContactType::TypeGroup);

        // check result for the default source
        QList<QContact> contacts = m_manager->contacts(filter);
        Q_FOREACH(const QContact &c, contacts) {
            QCOMPARE(c.type(), QContactType::TypeGroup);
            if (c.id().toString() == QStringLiteral("qtcontacts:galera::source@system-address-book")) {
                QContactDisplayLabel label = c.detail(QContactDisplayLabel::Type);
                QCOMPARE(label.label(), QStringLiteral("Personal"));
                return;
            }
        }
        QFAIL("Fail to query for collections");
    }
};

QTEST_MAIN(ContactCollectionTest)

#include "contact-collection-test.moc"
