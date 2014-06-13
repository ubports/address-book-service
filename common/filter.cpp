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

#include "filter.h"

#include <QtCore/QDataStream>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactIdFilter>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactUnionFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;

namespace galera
{

Filter::Filter(const QString &filter)
{
    m_filter = buildFilter(filter);
}

Filter::Filter(const QtContacts::QContactFilter &filter)
{
    m_filter = parseFilter(filter);
}

QString Filter::toString() const
{
    return toString(m_filter);
}

QtContacts::QContactFilter Filter::toContactFilter() const
{
    return m_filter;
}

bool Filter::test(const QContact &contact) const
{
    return QContactManagerEngine::testFilter(m_filter, contact);
}

bool Filter::checkIsValid(const QList<QContactFilter> filters) const
{
    Q_FOREACH(const QContactFilter &f, filters) {
        switch (f.type()) {
            case QContactFilter::InvalidFilter:
                return false;
            case QContactFilter::IntersectionFilter:
                return checkIsValid(static_cast<QContactIntersectionFilter>(f).filters());
            case QContactFilter::UnionFilter:
                return checkIsValid(static_cast<QContactUnionFilter>(f).filters());
            default:
                return true;
        }
    }
    // list is empty
    return true;
}

bool Filter::isValid() const
{
    return checkIsValid(QList<QContactFilter>() << m_filter);
}

bool Filter::checkIsEmpty(const QList<QContactFilter> filters) const
{
    Q_FOREACH(const QContactFilter &f, filters) {
        switch (f.type()) {
        case QContactFilter::DefaultFilter:
            return true;
        case QContactFilter::IntersectionFilter:
            return checkIsEmpty(static_cast<QContactIntersectionFilter>(f).filters());
        case QContactFilter::UnionFilter:
            return checkIsEmpty(static_cast<QContactUnionFilter>(f).filters());
        default:
            return false;
        }
    }
    // list is empty
    return true;
}

bool Filter::isEmpty() const
{
    return checkIsEmpty(QList<QContactFilter>() << m_filter);
}

QString Filter::toString(const QtContacts::QContactFilter &filter)
{
    QByteArray filterArray;
    QDataStream filterData(&filterArray, QIODevice::WriteOnly);

    filterData << filter;

    return QString::fromLatin1(filterArray.toBase64());
}

QtContacts::QContactFilter Filter::buildFilter(const QString &filter)
{
    QContactFilter filterObject;
    QByteArray filterArray = QByteArray::fromBase64(filter.toLatin1());
    QDataStream filterData(&filterArray, QIODevice::ReadOnly);
    filterData >> filterObject;
    return filterObject;
}

QtContacts::QContactFilter Filter::parseFilter(const QtContacts::QContactFilter &filter)
{
    QContactFilter newFilter;
    switch (filter.type()) {
    case QContactFilter::IdFilter:
        newFilter = parseIdFilter(filter);
        break;
    case QContactFilter::UnionFilter:
        newFilter = parseUnionFilter(filter);
        break;
    case QContactFilter::IntersectionFilter:
        newFilter = parseIntersectionFilter(filter);
        break;
    default:
        return filter;
    }
    return newFilter;
}

QtContacts::QContactFilter Filter::parseUnionFilter(const QtContacts::QContactFilter &filter)
{
    QContactUnionFilter newFilter;
    const QContactUnionFilter *unionFilter = static_cast<const QContactUnionFilter*>(&filter);
    Q_FOREACH(const QContactFilter &f, unionFilter->filters()) {
        newFilter << parseFilter(f);
    }
    return newFilter;
}

QtContacts::QContactFilter Filter::parseIntersectionFilter(const QtContacts::QContactFilter &filter)
{
    QContactIntersectionFilter newFilter;
    const QContactIntersectionFilter *intersectFilter = static_cast<const QContactIntersectionFilter*>(&filter);
    Q_FOREACH(const QContactFilter &f, intersectFilter->filters()) {
        newFilter << parseFilter(f);
    }
    return newFilter;
}

QtContacts::QContactFilter Filter::parseIdFilter(const QContactFilter &filter)
{
    // ContactId to be serialized between process is necessary to instantiate the manager in both sides.
    // Since the dbus service does not instantiate the manager we translate it to QContactDetailFilter
    // using Guid values. This is possible because our server use the Guid to build the contactId.
    const QContactIdFilter *idFilter = static_cast<const QContactIdFilter*>(&filter);
    if (idFilter->ids().isEmpty()) {
        return filter;
    }

    QContactUnionFilter newFilter;

    Q_FOREACH(const QContactId &id, idFilter->ids()) {
        QContactDetailFilter detailFilter;
        detailFilter.setMatchFlags(QContactFilter::MatchExactly);
        detailFilter.setDetailType(QContactDetail::TypeGuid, QContactGuid::FieldGuid);

        detailFilter.setValue(id.toString().split(":").last());
        newFilter << detailFilter;
    }
    return newFilter;
}


}
