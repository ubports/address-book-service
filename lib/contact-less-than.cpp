/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 * Copyright (C) 2013 Canonical Ltd.
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
 *
 * Based on QContactManagerEngine code by Digia
 */

#include "contact-less-than.h"
#include "contacts-map.h"
#include "qindividual.h"

#include <QtCore/QTime>
#include <QtCore/QDebug>

using namespace QtContacts;

namespace galera {

ContactLessThan::ContactLessThan(const galera::SortClause &sortClause)
    : m_sortClause(sortClause)
{
}

bool ContactLessThan::operator()(ContactEntry *entryA, ContactEntry *entryB)
{
    int r = compareContact(entryA->individual()->contact(),
                           entryB->individual()->contact(),
                           m_sortClause.toContactSortOrder());

    if (r != 0) {

    }
    return (r < 0);
}

int ContactLessThan::compareContact(const QContact &a,
                                    const QContact &b,
                                    const QList<QContactSortOrder>& sortOrders)
{
    Q_FOREACH(const QContactSortOrder& sortOrder, sortOrders) {
        if (!sortOrder.isValid())
            break;

        const QContactDetail::DetailType detailType = sortOrder.detailType();
        const int detailField = sortOrder.detailField();

        const QList<QContactDetail> aDetails = a.details(detailType);
        const QList<QContactDetail> bDetails = b.details(detailType);
        if (aDetails.isEmpty() && bDetails.isEmpty())
            continue; // use next sort criteria.

        // See if we need to check the values
        if (detailField == -1) {
            // just testing for the presence of a detail of the specified definition
            if (aDetails.size() == bDetails.size())
                continue; // use next sort criteria.
            if (aDetails.isEmpty())
                return sortOrder.blankPolicy() == QContactSortOrder::BlanksFirst ? -1 : 1;
            if (bDetails.isEmpty())
                return sortOrder.blankPolicy() == QContactSortOrder::BlanksFirst ? 1 : -1;
            return 0;
        }

        // obtain the values which this sort order concerns
        const QVariant aVal = !aDetails.isEmpty() ? aDetails.first().value(detailField) : QVariant();
        const QVariant bVal = !bDetails.isEmpty() ? bDetails.first().value(detailField) : QVariant();

        bool aIsNull = false;
        bool bIsNull = false;

        // treat empty strings as null qvariants.
        if ((aVal.type() == QVariant::String && aVal.toString().isEmpty()) || aVal.isNull()) {
            aIsNull = true;
        }
        if ((bVal.type() == QVariant::String && bVal.toString().isEmpty()) || bVal.isNull()) {
            bIsNull = true;
        }

        // early exit error checking
        if (aIsNull && bIsNull)
            continue; // use next sort criteria.
        if (aIsNull)
            return (sortOrder.blankPolicy() == QContactSortOrder::BlanksFirst ? -1 : 1);
        if (bIsNull)
            return (sortOrder.blankPolicy() == QContactSortOrder::BlanksFirst ? 1 : -1);

        // real comparison
        int comparison = compareVariant(aVal, bVal, sortOrder.caseSensitivity()) * (sortOrder.direction() == Qt::AscendingOrder ? 1 : -1);
        if (comparison == 0)
            continue;
        return comparison;
    }

    return 0; // or according to id? return (a.id() < b.id() ? -1 : 1);
}

int ContactLessThan::compareVariant(const QVariant& first, const QVariant& second, Qt::CaseSensitivity sensitivity)
{
    switch(first.type()) {
        case QVariant::Int:
            {
                const int a = first.toInt();
                const int b = second.toInt();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::LongLong:
            {
                const qlonglong a = first.toLongLong();
                const qlonglong b = second.toLongLong();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::Bool:
        case QVariant::UInt:
            {
                const uint a = first.toUInt();
                const uint b = second.toUInt();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::ULongLong:
            {
                const qulonglong a = first.toULongLong();
                const qulonglong b = second.toULongLong();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::Char: // Needs to do proper string comparison
        case QVariant::String:
            return compareStrings(first.toString(), second.toString(), sensitivity);

        case QVariant::Double:
            {
                const double a = first.toDouble();
                const double b = second.toDouble();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::DateTime:
            {
                const QDateTime a = first.toDateTime();
                const QDateTime b = second.toDateTime();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::Date:
            {
                const QDate a = first.toDate();
                const QDate b = second.toDate();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::Time:
            {
                const QTime a = first.toTime();
                const QTime b = second.toTime();
                return (a < b) ? -1 : ((a == b) ? 0 : 1);
            }

        case QVariant::StringList:
            {
                // We don't actually sort on these, I hope
                // {} < {"aa"} < {"aa","bb"} < {"aa", "cc"} < {"bb"}

                int i;
                const QStringList a = first.toStringList();
                const QStringList b = second.toStringList();
                for (i = 0; i < a.size(); i++) {
                    if (b.size() <= i)
                        return 1; // first is longer
                    int memberComp = compareStrings(a.at(i), b.at(i), sensitivity);
                    if (memberComp != 0)
                        return memberComp;
                    // this element is the same, so loop again
                }

                // Either a.size() < b.size() and they are equal all
                // the way, or a == b
                if (a.size() < b.size())
                    return -1; // a is less than b;
                return 0; // they are equal
            }

        default:
            return 0;
    }
}

int ContactLessThan::compareStrings(const QString& left, const QString& right, Qt::CaseSensitivity sensitivity)
{
    // strings starting with number is always greater
    bool leftIsLetter = left.at(0).isLetter();
    bool rightIsLetter = right.at(0).isLetter();

    if (leftIsLetter == rightIsLetter) {
        if (sensitivity == Qt::CaseSensitive) {
            return left.localeAwareCompare(right);
        } else {
            return left.toCaseFolded().localeAwareCompare(right.toCaseFolded());
        }
    } else {
        return (leftIsLetter ? -1 : 1);
    }
}

} // namespace
