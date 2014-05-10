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
        filters << QContactDetailFilter ();
        iFilter.setFilters(filters);

        Filter filter2(iFilter);
        QVERIFY(!filter2.isEmpty());
        QVERIFY(filter2.isValid());
    }
};

QTEST_MAIN(ClauseParseTest)

#include "clause-test.moc"
