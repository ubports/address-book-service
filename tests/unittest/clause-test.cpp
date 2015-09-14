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
        QTest::addColumn<bool>("match");            // if they are the same phone number
        QTest::addColumn<bool>("matchContains");    // if the phoneNumber contains query
        QTest::addColumn<bool>("matchStartsWith");  // if the phoneNumber starts with query
        QTest::addColumn<bool>("matchEndsWith");    // if the phoneNumber ends with the query
        QTest::addColumn<bool>("matchExactly");     // if the phoneNumber exacly match query

        QTest::newRow("string equal") << "12345678" << "12345678" << true << true << true << true << true;
        QTest::newRow("number with dash") << "1234-5678" << "12345678" << true << true << true << true << true;;
        QTest::newRow("number with country code") << "+55(81)87042155" << "(81)87042155" << true << true << false << true << false;
        QTest::newRow("number with and without country code") << "+55(81)87042155" << "(81)87042155" << true << true << false << true << false;
        QTest::newRow("number with and without area code") << "(81)87042155" << "87042155" << true << true << false << true << false;
        QTest::newRow("number with extension") << "12345678#123" << "12345678" << true << true << true << false << false;
        QTest::newRow("both numbers with extension") << "(123)12345678#1" << "12345678#1" << true << true << false << true << false;
        QTest::newRow("numbers with different extension") << "1234567#1" << "1234567#2" << false << false << false << false << false;
        QTest::newRow("short/emergency numbers") << "190" << "190" << true << true << true << true << true;
        QTest::newRow("different short/emergency numbers") << "911" << "11" << false << true << false << true << false;
        QTest::newRow("different numbers") << "12345678" << "1234567" << false << true << true << false << false;
        QTest::newRow("both non phone numbers") << "abcdefg" << "abcdefg" << false << false << false << false << false;
        QTest::newRow("different non phone numbers") << "abcdefg" << "bcdefg" << false << false << false << false << false;
        QTest::newRow("phone number and custom string") << "abc12345678" << "12345678" << true << true << true << true << false;
        QTest::newRow("Not match") << "+352 661 123456" << "+352 691 123456" << false << false << false << false << false;
        QTest::newRow("Phone number and small value") << "32634146" << "146" << false << true << false << true << false;
        QTest::newRow("Small phone numbers numbers") << "32634911" << "911" << false << true << false << true << false;
        QTest::newRow("Phone number and small number") << "32634911" << "+146" << false << false << false << false << false;
    }

    void testPhoneNumberFilter()
    {
        QFETCH(QString, phoneNumber);
        QFETCH(QString, query);
        QFETCH(bool, match);
        QFETCH(bool, matchContains);
        QFETCH(bool, matchStartsWith);
        QFETCH(bool, matchEndsWith);
        QFETCH(bool, matchExactly);

        QContact c;
        QContactPhoneNumber p;
        p.setNumber(phoneNumber);
        c.saveDetail(&p);

        // if they are the same phone number
        QContactDetailFilter f = QContactPhoneNumber::match(query);
        Filter myFilter(f);
        QCOMPARE(myFilter.test(c), match);

        // if the phoneNumber contains query
        f.setMatchFlags(QContactFilter::MatchPhoneNumber | QContactFilter::MatchContains);
        myFilter = Filter(f);
        QCOMPARE(myFilter.test(c), matchContains);

        // if the phoneNumber starts with query
        f.setMatchFlags(QContactFilter::MatchPhoneNumber | QContactFilter::MatchStartsWith);
        myFilter = Filter(f);
        QCOMPARE(myFilter.test(c), matchStartsWith);

        // if the phoneNumber ends with the query
        f.setMatchFlags(QContactFilter::MatchPhoneNumber | QContactFilter::MatchEndsWith);
        myFilter = Filter(f);
        QCOMPARE(myFilter.test(c), matchEndsWith);

        // Exactly the same number
        f.setMatchFlags(QContactFilter::MatchPhoneNumber | QContactFilter::MatchExactly);
        myFilter = Filter(f);
        // Failing due the MatchFlag problem check: https://bugreports.qt.io/browse/QTBUG-47546
        //QCOMPARE(myFilter.test(c), matchExactly);
    }

    void testExtractIds()
    {
        QContactIdFilter originalFilter;
        originalFilter.setIds(QList<QContactId>()
                              << QContactId::fromString("qtcontacts:memory::1")
                              << QContactId::fromString("qtcontacts:memory::2")
                              << QContactId::fromString("qtcontacts:memory::3"));

        Filter f(originalFilter);

        QStringList ids = f.idsToFilter();
        QCOMPARE(ids.size(), 3);

        Q_FOREACH(const QContactId &id, originalFilter.ids()) {
            ids.removeOne(id.toString().split(":").last());
        }
        QCOMPARE(ids.size(), 0);
    }

    void testShowInvisibleContactsForButeo()
    {
        QContactIntersectionFilter originalFilter;
        QCOMPARE(Filter(originalFilter).showInvisibleContacts(), false);


        QContactDetailFilter xDetailNameFilter;
        xDetailNameFilter.setDetailType(QContactExtendedDetail::Type,
                                        QContactExtendedDetail::FieldName);
        xDetailNameFilter.setValue("X-REMOTE-ID");

        QContactDetailFilter xDetailValueFilter;
        xDetailValueFilter.setDetailType(QContactExtendedDetail::Type,
                                         QContactExtendedDetail::FieldData);
        xDetailValueFilter.setValue("123");

        originalFilter << xDetailNameFilter
                        << xDetailValueFilter;

        QCOMPARE(Filter(originalFilter).showInvisibleContacts(), true);

        QContactDetailFilter detailFilterDefaultSyncTarget;
        detailFilterDefaultSyncTarget.setDetailType(QContactSyncTarget::Type,
                                                    QContactSyncTarget::FieldSyncTarget + 1);
        detailFilterDefaultSyncTarget.setValue("my-sync-target");
        QCOMPARE(Filter(originalFilter & detailFilterDefaultSyncTarget).showInvisibleContacts(), true);
    }
};

QTEST_MAIN(ClauseParseTest)

#include "clause-test.moc"
