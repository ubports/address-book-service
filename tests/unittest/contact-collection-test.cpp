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

#include "base-eds-test.h"
#include "config.h"


using namespace QtContacts;

class ContactCollectionTest : public QObject, public BaseEDSTest
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        BaseEDSTest::initTestCaseImpl();
    }

    void cleanupTestCase()
    {
        BaseEDSTest::cleanupTestCaseImpl();
    }

    void init()
    {
        BaseEDSTest::initImpl();
    }

    void cleanup()
    {
        BaseEDSTest::cleanupImpl();
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

    /*
     * Test edit collection
     */
    void testEditAddressBook()
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

        QContactDetailFilter collectionFilter;
        collectionFilter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
        collectionFilter.setValue(QContactType::TypeGroup);

        // current collection size
        int collectionSize = m_manager->contacts(collectionFilter).size();

        label = c.detail<QContactDisplayLabel>();
        QString newUniqueName = QString("NEW_%1").arg(uniqueName);
        label.setLabel(newUniqueName);
        c.saveDetail(&label);

        bool isPrimaryUpdated = false;
        Q_FOREACH(QContactExtendedDetail xDetail, c.details<QContactExtendedDetail>()) {
            if (xDetail.name() == "IS-PRIMARY") {
                xDetail.setData(true);
                c.saveDetail(&xDetail);
                isPrimaryUpdated = true;
                break;
            }
        }
        QVERIFY(isPrimaryUpdated);

        saveResult = m_manager->saveContact(&c);
        QVERIFY(saveResult);

        // check if the collections contains the new collection
        QList<QContact> contacts = m_manager->contacts(collectionFilter);
        QCOMPARE(contacts.size() , collectionSize);

        Q_FOREACH(const QContact &contact, contacts) {
            QVERIFY(c.id().toString().startsWith("qtcontacts:galera::source@"));
            if (contact.detail<QContactDisplayLabel>().label() == newUniqueName) {
                bool isPrimary = false;
                Q_FOREACH(QContactExtendedDetail xDetail, c.details<QContactExtendedDetail>()) {
                    if (xDetail.name() == "IS-PRIMARY") {
                        isPrimary = xDetail.data().toBool();
                    }
                }
                QVERIFY(isPrimary);
                return;
            }
        }
        QFAIL("New collection not found");
    }
};

QTEST_MAIN(ContactCollectionTest)

#include "contact-collection-test.moc"
