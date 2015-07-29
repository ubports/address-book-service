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

#include "common/filter.h"

using namespace QtContacts;
using namespace galera;

class ClauseParseTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testParseClause()
    {
        // Create manager to allow us to creact contact id
        QContactManager manager("memory");

        QContactId id5 = QContactId::fromString("qtcontacts:memory::5");
        QContactId id2 = QContactId::fromString("qtcontacts:memory::2");
        QContactGuid guid5;
        guid5.setGuid("5");

        QContactGuid guid2;
        guid2.setGuid("2");

        // guid is necessary because our server uses that for compare ids
        // check Filter source code for more details
        QContact contact5;
        contact5.setId(id5);
        contact5.appendDetail(guid5);

        QContact contact2;
        contact2.setId(id2);
        contact2.appendDetail(guid2);

        QContactIdFilter originalFilter = QContactIdFilter();
        originalFilter.setIds(QList<QContactId>()
                              << QContactId::fromString("qtcontacts:memory::1")
                              << QContactId::fromString("qtcontacts:memory::2")
                              << QContactId::fromString("qtcontacts:memory::3"));

        QString filterString = Filter(originalFilter).toString();
        Filter filterFromString = Filter(filterString);

        QCOMPARE(filterFromString.test(contact5), false);
        QCOMPARE(filterFromString.test(contact2), true);
        QCOMPARE(filterFromString.test(contact5), QContactManagerEngine::testFilter(originalFilter, contact5));
        QCOMPARE(filterFromString.test(contact2), QContactManagerEngine::testFilter(originalFilter, contact2));
    }

    void testIntersectionWithContactId()
    {
        // Create manager to allow us to creact contact id
        QContactManager manager("memory");

        QContactId id5 = QContactId::fromString("qtcontacts:memory::5");
        QContactId id2 = QContactId::fromString("qtcontacts:memory::2");
        QContactGuid guid5;
        guid5.setGuid("5");

        QContactGuid guid2;
        guid2.setGuid("2");

        // guid is necessary because our server uses that for compare ids
        // check Filter source code for more details
        QContact contact5;
        contact5.setId(id5);
        contact5.appendDetail(guid5);

        QContact contact2;
        contact2.setId(id2);
        contact2.appendDetail(guid2);

        QContactIntersectionFilter iFilter;
        QContactIdFilter originalFilter = QContactIdFilter();
        originalFilter.setIds(QList<QContactId>()
                              << QContactId::fromString("qtcontacts:memory::1")
                              << QContactId::fromString("qtcontacts:memory::2")
                              << QContactId::fromString("qtcontacts:memory::3"));
        iFilter.setFilters(QList<QContactFilter>() << originalFilter);

        QString filterString = Filter(iFilter).toString();
        Filter filterFromString = Filter(filterString);

        QCOMPARE(filterFromString.test(contact5), false);
        QCOMPARE(filterFromString.test(contact2), true);
        QCOMPARE(filterFromString.test(contact5), QContactManagerEngine::testFilter(iFilter, contact5));
        QCOMPARE(filterFromString.test(contact2), QContactManagerEngine::testFilter(iFilter, contact2));
    }

    void testIntersecionListFilter()
    {
        // Create manager to allow us to creact contact id
        QContactManager manager("memory");

        QContactId id5 = QContactId::fromString("qtcontacts:memory::5");
        QContactId id2 = QContactId::fromString("qtcontacts:memory::2");
        QContactGuid guid5;
        guid5.setGuid("5");

        QContactGuid guid2;
        guid2.setGuid("2");

        // guid is necessary because our server uses that for compare ids
        // check Filter source code for more details
        QContact contact5;
        contact5.setId(id5);
        contact5.appendDetail(guid5);

        QContact contact2;
        contact2.setId(id2);
        contact2.appendDetail(guid2);

        QContactIntersectionFilter iFilter;
        QContactIntersectionFilter emptyField;
        QContactIdFilter originalFilter = QContactIdFilter();
        originalFilter.setIds(QList<QContactId>()
                              << QContactId::fromString("qtcontacts:memory::1")
                              << QContactId::fromString("qtcontacts:memory::2")
                              << QContactId::fromString("qtcontacts:memory::3"));
        iFilter.setFilters(QList<QContactFilter>() << originalFilter << emptyField);

        QString filterString = Filter(iFilter).toString();
        Filter filterFromString = Filter(filterString);

        QCOMPARE(filterFromString.test(contact5), QContactManagerEngine::testFilter(iFilter, contact5));
        QCOMPARE(filterFromString.test(contact2), QContactManagerEngine::testFilter(iFilter, contact2));

        QCOMPARE(filterFromString.test(contact5), false);
        QCOMPARE(filterFromString.test(contact2), false);
    }

    void testSimpleEmptyFilter()
    {
        Filter filter("");
        QVERIFY(filter.isEmpty());
        QVERIFY(filter.isValid());
    }

    void testUnionEmptyFilter()
    {
        QContactUnionFilter uFilter;
        Filter filter(uFilter);
        QVERIFY(filter.isEmpty());
        QVERIFY(filter.isValid());

        QList<QContactFilter> filters;
        filters << QContactDetailFilter();
        uFilter.setFilters(filters);

        Filter filter2(uFilter);
        QVERIFY(!filter2.isEmpty());
        QVERIFY(filter2.isValid());
    }

    void testIntersectEmptyFilter()
    {
        QContactIntersectionFilter iFilter;
        Filter filter(iFilter);
        QVERIFY(filter.isEmpty());
        QVERIFY(filter.isValid());

        QList<QContactFilter> filters;
        filters << QContactDetailFilter();
        iFilter.setFilters(filters);

        Filter filter2(iFilter);
        QVERIFY(!filter2.isEmpty());
        QVERIFY(filter2.isValid());
    }

    void testPhoneNumberFilter_data()
    {
        QTest::addColumn<QString>("phoneNumber");
        QTest::addColumn<QString>("query");
        QTest::addColumn<bool>("expectedResult");

        QTest::newRow("string equal") << "12345678" << "12345678" << true;
        QTest::newRow("number with dash") << "1234-5678" << "12345678" << true;
        QTest::newRow("number with area code") << "1231234567" << "1234567" << true;
        QTest::newRow("number with extension") << "12345678#123" << "12345678" << true;
        QTest::newRow("both numbers with extension") << "(123)12345678#1" << "12345678#1" << true;
        QTest::newRow("numbers with different extension") << "1234567#1" << "1234567#2" << false;
        QTest::newRow("short/emergency numbers") << "190" << "190" << true;
        QTest::newRow("different short/emergency numbers") << "911" << "11" << false;
        QTest::newRow("different numbers") << "12345678" << "1234567" << false;
        QTest::newRow("both non phone numbers") << "abcdefg" << "abcdefg" << false;
        QTest::newRow("different non phone numbers") << "abcdefg" << "bcdefg" << false;
        QTest::newRow("phone number and custom string") << "abc12345678" << "12345678" << true;
        QTest::newRow("Not match") << "+352 661 123456" << "+352 691 123456" << false;
    }

    void testPhoneNumberFilter()
    {
        QFETCH(QString, phoneNumber);
        QFETCH(QString, query);
        QFETCH(bool, expectedResult);

        QContact c;
        QContactPhoneNumber p;
        p.setNumber(phoneNumber);
        c.saveDetail(&p);

        QContactFilter f = QContactPhoneNumber::match(query);
        Filter myFilter(f);
        QCOMPARE(myFilter.test(c), expectedResult);
    }
};

QTEST_MAIN(ClauseParseTest)

#include "clause-test.moc"
