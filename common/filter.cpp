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
#include <QtCore/QLocale>
#include <QtCore/QDebug>

#include <QtContacts/QContactGuid>
#include <QtContacts/QContactExtendedDetail>
#include <QtContacts/QContactIdFilter>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactUnionFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactChangeLogFilter>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactIdFilter>
#include <QtContacts/QContactRelationshipFilter>

#include <phonenumbers/phonenumberutil.h>

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

bool Filter::test(const QContact &contact, const QDateTime &deletedDate) const
{
    if (deletedDate.isValid() && !includeRemoved()) {
        return false;
    }

    return testFilter(m_filter, contact, deletedDate);
}

bool Filter::testFilter(const QContactFilter& filter,
                        const QContact &contact,
                        const QDateTime &deletedDate)
{
    switch(filter.type()) {
        case QContactFilter::IdFilter:
            return QContactManagerEngine::testFilter(filter, contact);

        case QContactFilter::ChangeLogFilter:
        {
            const QContactChangeLogFilter bf(filter);
            if (bf.eventType() == QContactChangeLogFilter::EventRemoved) {
                return (deletedDate >= bf.since());
            } else if (QContactManagerEngine::testFilter(filter, contact)) {
                return true;
            }
            break;
        }

        case QContactFilter::ContactDetailFilter:
        {
            const QContactDetailFilter cdf(filter);
            if (cdf.detailType() == QContactDetail::TypeUndefined)
                return false;

            /* See if this contact has one of these details in it */
            const QList<QContactDetail>& details = contact.details(cdf.detailType());

            if (details.count() == 0)
                return false; /* can't match */

            /* See if we need to check the values */
            if (cdf.detailField() == -1)
                return true;  /* just testing for the presence of a detail of the specified type */

            if (cdf.matchFlags() & QContactFilter::MatchPhoneNumber) {
                /* Doing phone number filtering.  We hand roll an implementation here, backends will obviously want to override this. */
                QString input = cdf.value().toString();

                /* Look at every detail in the set of details and compare */
                for (int j = 0; j < details.count(); j++) {
                    const QContactDetail& detail = details.at(j);
                    const QString& valueString = detail.value(cdf.detailField()).toString();

                    if (comparePhoneNumbers(input, valueString, cdf.matchFlags())) {
                        return true;
                    }
                }
            } else if (QContactManagerEngine::testFilter(cdf, contact)) {
                return true;
            }
            break;
        }

        case QContactFilter::IntersectionFilter:
        {
            /* XXX In theory we could reorder the terms to put the native tests first */
            const QContactIntersectionFilter bf(filter);
            const QList<QContactFilter>& terms = bf.filters();

            if (terms.isEmpty()) {
                break;
            }

           Q_FOREACH(const QContactFilter &f, terms) {
                if (!testFilter(f, contact, deletedDate)) {
                    return false;
                }
            }
            return true;
        }
        break;

        case QContactFilter::UnionFilter:
        {
            /* XXX In theory we could reorder the terms to put the native tests first */
            const QContactUnionFilter bf(filter);
            const QList<QContactFilter>& terms = bf.filters();
            if (terms.count() > 0) {
                for(int j = 0; j < terms.count(); j++) {
                    if (testFilter(terms.at(j), contact, deletedDate)) {
                        return true;
                    }
                }
                return false;
            }
            // Fall through to end
        }
        break;

        default:
            if (QContactManagerEngine::testFilter(filter, contact)) {
                return true;
            }
            break;

    }

    return false;
}

bool Filter::comparePhoneNumbers(const QString &input, const QString &value, QContactFilter::MatchFlags flags)
{
    static i18n::phonenumbers::PhoneNumberUtil *phonenumberUtil = i18n::phonenumbers::PhoneNumberUtil::GetInstance();

    std::string stdPreprocessedInput(input.toStdString());
    std::string stdProcessedValue(value.toStdString());

    phonenumberUtil->NormalizeDiallableCharsOnly(&stdPreprocessedInput);
    phonenumberUtil->NormalizeDiallableCharsOnly(&stdProcessedValue);

    QString preprocessedInput = QString::fromStdString(stdPreprocessedInput);
    QString preprocessedValue = QString::fromStdString(stdProcessedValue);

    // if one of they does not contain digits return false
    if (preprocessedInput.isEmpty() || preprocessedValue.isEmpty()) {
        return false;
    }

    bool mc = flags & QContactFilter::MatchContains;
    bool msw = flags & QContactFilter::MatchStartsWith;
    bool mew = flags & QContactFilter::MatchEndsWith;
    bool me = flags & QContactFilter::MatchExactly;
    if (!mc && !msw && !mew && !me &&
        ((preprocessedInput.length() < 6) || (preprocessedValue.length() < 6))) {
        return preprocessedInput == preprocessedValue;
    }

    if (mc) {
        return preprocessedValue.contains(preprocessedInput);
    } else if (msw) {
        return preprocessedValue.startsWith(preprocessedInput);
    } else if (mew) {
        return preprocessedValue.endsWith(preprocessedInput);
    } else {
        i18n::phonenumbers::PhoneNumberUtil::MatchType match =
                phonenumberUtil->IsNumberMatchWithTwoStrings(input.toStdString(),
                                                             value.toStdString());
        if (me) {
            return match == i18n::phonenumbers::PhoneNumberUtil::EXACT_MATCH;
        } else {
            return match > i18n::phonenumbers::PhoneNumberUtil::NO_MATCH;
        }
    }
    return false;
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

bool Filter::isIdFilter(const QContactFilter &filter) const
{
    if (filter.type() == QContactFilter::IdFilter) {
        return true;
    }

    // FIXME: We convert IdFilter to GUID filter check Filter implementation
    if (filter.type() == QContactFilter::UnionFilter) {
        QContactUnionFilter uFilter(filter);
        if ((uFilter.filters().size() == 1) &&
            (uFilter.filters().at(0).type() == QContactFilter::ContactDetailFilter)) {
            QContactDetailFilter dFilter(uFilter.filters().at(0));
            return ((dFilter.detailType() == QContactDetail::TypeGuid) &&
                    (dFilter.detailField() == QContactGuid::FieldGuid));
        }
    }

    return false;
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

bool Filter::includeRemoved() const
{
    // FIXME: Return deleted contacts for id filter
    // we need this to avoid problems with buteo, we should fix buteo to query for any contact
    // include deleted ones.
    if (isIdFilter(m_filter)) {
        return true;
    }
    return includeRemoved(m_filter);
}

QString Filter::phoneNumberToFilter() const
{
    return phoneNumberToFilter(m_filter);
}

QStringList Filter::idsToFilter() const
{
    return idsToFilter(m_filter);
}

QString Filter::phoneNumberToFilter(const QtContacts::QContactFilter &filter)
{
    switch (filter.type()) {
    case QContactFilter::ContactDetailFilter:
    {
        const QContactDetailFilter cdf(filter);
        if (cdf.matchFlags() & QContactFilter::MatchPhoneNumber) {
            return cdf.value().toString();
        }
        break;
    }
    case QContactFilter::UnionFilter:
    {
        // if the union contains only the phone filter we'are still able to optimize
        const QContactUnionFilter uf(filter);
        if (uf.filters().size() == 1) {
            return phoneNumberToFilter(uf.filters().first());
        }
        break;
    }
    case QContactFilter::IntersectionFilter:
    {
        const QContactIntersectionFilter cif(filter);
        Q_FOREACH(const QContactFilter &f, cif.filters()) {
            QString phoneToFilter = phoneNumberToFilter(f);
            if (!phoneToFilter.isEmpty()) {
                return phoneToFilter;
            }
        }
        break;
    }
    default:
        break;
    }
    return QString();
}

QStringList Filter::idsToFilter(const QtContacts::QContactFilter &filter)
{
    QStringList result;

    switch (filter.type()) {
    case QContactFilter::ContactDetailFilter:
    {
        const QContactDetailFilter cdf(filter);
        if ((cdf.detailType() == QContactDetail::TypeGuid) &&
            (cdf.detailField() == QContactGuid::FieldGuid) &&
            cdf.matchFlags().testFlag(QContactFilter::MatchExactly)) {
            result << cdf.value().toString();
        }
        break;
    }
    case QContactFilter::IdFilter:
    {

        const QContactIdFilter idf(filter);
        Q_FOREACH(const QContactId &id, idf.ids()) {
            result << id.toString();
        }
        break;
    }
    case QContactFilter::UnionFilter:
    {
        const QContactUnionFilter uf(filter);
        Q_FOREACH(const QContactFilter &f, uf.filters()) {
            result.append(idsToFilter(f));
        }
        break;
    }
    case QContactFilter::IntersectionFilter:
    {
        const QContactIntersectionFilter cif(filter);
        Q_FOREACH(const QContactFilter &f, cif.filters()) {
            result.append(idsToFilter(f));
        }
        break;
    }
    default:
        break;
    }
    return result;
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

bool Filter::includeRemoved(const QList<QContactFilter> filters)
{
    Q_FOREACH(const QContactFilter &f, filters) {
        if (includeRemoved(f)) {
            return true;
        }
    }
    return false;
}

//  we will include removed contacts if the filter contains ChangeLogFilter with type EventRemoved;
bool Filter::includeRemoved(const QContactFilter &filter)
{
    if (filter.type() == QContactFilter::ChangeLogFilter) {
        QContactChangeLogFilter fChangeLog(filter);
        return (fChangeLog.eventType() == QContactChangeLogFilter::EventRemoved);
    } else if (filter.type() == QContactFilter::UnionFilter) {
        return includeRemoved(QContactUnionFilter(filter).filters());
    } else if (filter.type() == QContactFilter::IntersectionFilter) {
        return includeRemoved(QContactIntersectionFilter(filter).filters());
    } else {
        return false;
    }
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
    const QContactIntersectionFilter intersectFilter(filter);
    Q_FOREACH(const QContactFilter &f, intersectFilter.filters()) {
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
