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

#include "common/sort-clause.h"

using namespace QtContacts;
using namespace galera;

class SortClauseTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testSingleClause()
    {
        const QString strClause = QString("FIRST_NAME ASC");
        SortClause clause(strClause);

        QList<QContactSortOrder> cClauseList;
        QContactSortOrder cClause;
        cClause.setCaseSensitivity(Qt::CaseInsensitive);
        cClause.setDetailType(QContactDetail::TypeName, QContactName::FieldFirstName);
        cClauseList << cClause;

        QVERIFY(clause.toContactSortOrder() == cClauseList);

        SortClause clause2(cClauseList);
        QCOMPARE(clause2.toString(), strClause);
    }

    void testComplexClause()
    {
        const QString strClause = QString("FIRST_NAME ASC, ORG_DEPARTMENT, ADDR_STREET DESC, URL");
        const QString strClauseFull = QString("FIRST_NAME ASC, ORG_DEPARTMENT ASC, ADDR_STREET DESC, URL ASC");
        SortClause clause(strClause);

        QList<QContactSortOrder> cClauseList;

        QContactSortOrder sortFirstName;
        sortFirstName.setDetailType(QContactDetail::TypeName, QContactName::FieldFirstName);
        sortFirstName.setDirection(Qt::AscendingOrder);
        sortFirstName.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseList << sortFirstName;

        QContactSortOrder sortDepartment;
        sortDepartment.setDetailType(QContactDetail::TypeOrganization, QContactOrganization::FieldDepartment);
        sortDepartment.setDirection(Qt::AscendingOrder);
        sortDepartment.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseList << sortDepartment;

        QContactSortOrder sortStreet;
        sortStreet.setDetailType(QContactDetail::TypeAddress, QContactAddress::FieldStreet);
        sortStreet.setDirection(Qt::DescendingOrder);
        sortStreet.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseList << sortStreet;

        QContactSortOrder sortUrl;
        sortUrl.setDetailType(QContactDetail::TypeUrl, QContactUrl::FieldUrl);
        sortUrl.setDirection(Qt::AscendingOrder);
        sortUrl.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseList << sortUrl;

        QVERIFY(clause.toContactSortOrder() == cClauseList);

        SortClause clause2(cClauseList);
        QCOMPARE(clause2.toString(), strClauseFull);
    }

    // should ignore the first field
    void testInvalidFieldNameClause()
    {
        SortClause clause("FIRSTNAME ASC, URL");
        QCOMPARE(clause.toString(), QString("URL ASC"));

        QList<QContactSortOrder> cClauseList;

        QContactSortOrder sortUrl;
        sortUrl.setDetailType(QContactDetail::TypeUrl, QContactUrl::FieldUrl);
        sortUrl.setDirection(Qt::AscendingOrder);
        sortUrl.setCaseSensitivity(Qt::CaseInsensitive);
        cClauseList << sortUrl;

        QVERIFY(clause.toContactSortOrder() == cClauseList);
    }

    void testInvalidSintaxClause()
    {
        SortClause clause("FIRST_NAME ASC URL");
        QCOMPARE(clause.toString(), QString(""));
        QCOMPARE(clause.toContactSortOrder().size(), 0);
    }

};

QTEST_MAIN(SortClauseTest)

#include "sort-clause-test.moc"
