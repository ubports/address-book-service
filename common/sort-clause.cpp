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

#include "sort-clause.h"

#include <QtContacts/qcontactdetails.h>
#include <QtContacts/QContactSortOrder>
#include <QtContacts/QContactManagerEngine>

using namespace QtContacts;

namespace galera
{

static QMap<QString, QPair<QContactDetail::DetailType, int> > clauseFieldMap;

SortClause::SortClause(const QString &sort)
{
    initialize();
    Q_FOREACH(QString sortClause, sort.split(",")) {
        QContactSortOrder sOrder = fromString(sortClause);
        if (sOrder.isValid()) {
            m_sortOrders << sOrder;
        }
    }
}

SortClause::SortClause(QList<QtContacts::QContactSortOrder> sort)
    : m_sortOrders(sort)
{
    initialize();
}

SortClause::SortClause(const SortClause &other)
    : m_sortOrders(other.m_sortOrders)
{
    initialize();
}

QString SortClause::toString() const
{
    QString result;
    Q_FOREACH(QContactSortOrder sortOrder, m_sortOrders) {
        result += toString(sortOrder) + ", ";
    }
    if (result.endsWith(", ")) {
        result = result.mid(0, result.size() - 2);
    }
    return result;
}

QtContacts::QContactSortOrder SortClause::fromString(const QString &clause) const
{
    QStringList sort = clause.trimmed().split(" ");
    if ((sort.count() == 0) || (sort.count() > 2)) {
        qWarning() << "Invalid sort clause:" << clause;
        return QContactSortOrder();
    }
    QString fieldName = sort[0].trimmed().toUpper();
    QString orderName = (sort.count() == 2 ? sort[1].trimmed().toUpper() : "ASC");

    QtContacts::QContactSortOrder sOrder;
    if (clauseFieldMap.contains(fieldName)) {
        QPair<QContactDetail::DetailType, int> details = clauseFieldMap[fieldName];
        sOrder.setDetailType(details.first, details.second);

        Qt::SortOrder order = (orderName == "DESC" ? Qt::DescendingOrder : Qt::AscendingOrder);
        sOrder.setDirection(order);
        sOrder.setCaseSensitivity(Qt::CaseInsensitive);
        return sOrder;
    } else {
        qWarning() << "Invalid sort field:" << sort[0];
        return QContactSortOrder();
    }
}

QList<QtContacts::QContactSortOrder> SortClause::toContactSortOrder() const
{
    return m_sortOrders;
}

QStringList SortClause::supportedFields()
{
    initialize();
    return clauseFieldMap.keys();
}

void SortClause::initialize()
{
    if (clauseFieldMap.isEmpty()) {
        clauseFieldMap["NAME_PREFIX"]   = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeName,           QContactName::FieldPrefix);
        clauseFieldMap["FIRST_NAME"]    = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeName,           QContactName::FieldFirstName);
        clauseFieldMap["MIDLE_NAME"]    = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeName,           QContactName::FieldMiddleName);
        clauseFieldMap["LAST_NAME"]     = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeName,           QContactName::FieldLastName);
        clauseFieldMap["NAME_SUFFIX"]   = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeName,           QContactName::FieldSuffix);
        clauseFieldMap["FULL_NAME"]     = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeDisplayLabel,   QContactDisplayLabel::FieldLabel);
        clauseFieldMap["NICKNAME"]      = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeNickname,       QContactNickname::FieldNickname);
        clauseFieldMap["BIRTHDAY"]      = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeBirthday,       QContactBirthday::FieldBirthday);
        clauseFieldMap["PHOTO"]         = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAvatar,         QContactAvatar::FieldImageUrl);
        clauseFieldMap["ORG_ROLE"]      = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOrganization,   QContactOrganization::FieldRole);
        clauseFieldMap["ORG_NAME"]      = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOrganization,   QContactOrganization::FieldName);
        clauseFieldMap["ORG_DEPARTMENT"]= QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOrganization,   QContactOrganization::FieldDepartment);
        clauseFieldMap["ORG_LOCATION"]  = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOrganization,   QContactOrganization::FieldLocation);
        clauseFieldMap["ORG_TITLE"]     = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOrganization,   QContactOrganization::FieldTitle);
        clauseFieldMap["EMAIL"]         = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeEmailAddress,   QContactEmailAddress::FieldEmailAddress);
        clauseFieldMap["PHONE"]         = QPair<QContactDetail::DetailType, int>(QContactDetail::TypePhoneNumber,    QContactPhoneNumber::FieldNumber);
        clauseFieldMap["ADDR_STREET"]   = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress,        QContactAddress::FieldStreet);
        clauseFieldMap["ADDR_LOCALITY"] = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress,        QContactAddress::FieldLocality);
        clauseFieldMap["ADDR_REGION"]   = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress,        QContactAddress::FieldRegion);
        clauseFieldMap["ADDR_COUNTRY"]  = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress,        QContactAddress::FieldCountry);
        clauseFieldMap["ADDR_POSTCODE"] = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress,        QContactAddress::FieldPostcode);
        clauseFieldMap["ADDR_POST_OFFICE_BOX"] = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeAddress, QContactAddress::FieldPostOfficeBox);
        clauseFieldMap["IM_URI"]        = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOnlineAccount,  QContactOnlineAccount::FieldAccountUri);
        clauseFieldMap["IM_PROTOCOL"]   = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeOnlineAccount,  QContactOnlineAccount::FieldProtocol);
        clauseFieldMap["URL"]           = QPair<QContactDetail::DetailType, int>(QContactDetail::TypeUrl,            QContactUrl::FieldUrl);
    }
}

QString SortClause::toString(const QContactSortOrder &sort) const
{
    QPair<QContactDetail::DetailType, int> clausePair = qMakePair(sort.detailType(), sort.detailField());

    Q_FOREACH(QString key, clauseFieldMap.keys()) {
        if (clauseFieldMap[key] == clausePair) {
            return key + (sort.direction() == Qt::AscendingOrder ? " ASC" : " DESC");
        }
    }

    return "";
}

}
