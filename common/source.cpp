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

#include <QtContacts/QContactGuid>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactExtendedDetail>

#include "source.h"

using namespace QtContacts;

namespace galera {

Source::Source()
    : m_accountId(0),
      m_isReadOnly(false),
      m_isPrimary(false)
{
}

Source::Source(const Source &other)
    : m_id(other.id()),
      m_displayName(other.displayLabel()),
      m_applicationId(other.applicationId()),
      m_providerName(other.providerName()),
      m_isReadOnly(other.isReadOnly()),
      m_isPrimary(other.isPrimary()),
      m_accountId(other.accountId())
{
}

Source::Source(QString id,
               const QString &displayName,
               const QString &applicationId,
               const QString &providerName,
               uint accountId,
               bool isReadOnly,
               bool isPrimary)
    : m_id(id),
      m_displayName(displayName),
      m_applicationId(applicationId),
      m_providerName(providerName),
      m_accountId(accountId),
      m_isReadOnly(isReadOnly),
      m_isPrimary(isPrimary)
{
    Q_ASSERT(displayName.isEmpty() == false);
}

bool Source::isValid() const
{
    return !m_id.isEmpty();
}

bool Source::isPrimary() const
{
    return m_isPrimary;
}

uint Source::accountId() const
{
    return m_accountId;
}

QString Source::applicationId() const
{
    return m_applicationId;
}

QString Source::providerName() const
{
    return m_providerName;
}

QString Source::id() const
{
    return m_id;
}

QString Source::displayLabel() const
{
    return m_displayName;
}

bool Source::isReadOnly() const
{
    return m_isReadOnly;
}

void Source::registerMetaType()
{
    qRegisterMetaType<Source>("Source");
    qRegisterMetaType<SourceList>("SourceList");
    qDBusRegisterMetaType<Source>();
    qDBusRegisterMetaType<SourceList>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Source &source)
{
    argument.beginStructure();
    argument << source.m_id;
    argument << source.m_displayName;
    argument << source.m_providerName;
    argument << source.m_applicationId;
    argument << source.m_accountId;
    argument << source.m_isReadOnly;
    argument << source.m_isPrimary;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Source &source)
{
    argument.beginStructure();
    argument >> source.m_id;
    argument >> source.m_displayName;
    argument >> source.m_providerName;
    argument >> source.m_applicationId;
    argument >> source.m_accountId;
    argument >> source.m_isReadOnly;
    argument >> source.m_isPrimary;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SourceList &sources)
{
    argument.beginArray(qMetaTypeId<Source>());
    for(int i=0; i < sources.count(); ++i) {
        argument << sources[i];
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SourceList &sources)
{
    argument.beginArray();
    sources.clear();
    while(!argument.atEnd()) {
        Source source;
        argument >> source;
        sources << source;
    }
    argument.endArray();
    return argument;
}

QtContacts::QContact Source::toContact(const QContactId &id) const
{
    QContact contact;

    // contact group type
    contact.setType(QContactType::TypeGroup);

    // id
    contact.setId(id);

    // guid
    QContactGuid guid;
    guid.setGuid(m_id);
    contact.saveDetail(&guid);

    // display name
    QContactDisplayLabel displayLabel;
    displayLabel.setLabel(m_displayName);
    contact.saveDetail(&displayLabel);

    // read-only
    QContactExtendedDetail readOnly;
    readOnly.setName("READ-ONLY");
    readOnly.setData(m_isReadOnly);
    contact.saveDetail(&readOnly);

    // Primary
    QContactExtendedDetail primary;
    primary.setName("IS-PRIMARY");
    primary.setData(m_isPrimary);
    contact.saveDetail(&primary);

    // Account Id
    QContactExtendedDetail accountId;
    accountId.setName("ACCOUNT-ID");
    accountId.setData(m_accountId);
    contact.saveDetail(&accountId);

    // Application Id
    QContactExtendedDetail applicationId;
    applicationId.setName("APPLICATION-ID");
    applicationId.setData(m_applicationId);
    contact.saveDetail(&applicationId);

    // Provider
    QContactExtendedDetail provider;
    provider.setName("PROVIDER");
    provider.setData(m_providerName);
    contact.saveDetail(&provider);

    return contact;
}

Source Source::fromQContact(const QtContacts::QContact &contact)
{
    if (contact.type() != QContactType::TypeGroup) {
        qWarning() << "Invalid contact type";
        return Source();
    }

    QContactGuid guid = contact.detail<QContactGuid>();
    QContactDisplayLabel displayLabel = contact.detail<QContactDisplayLabel>();

    bool isReadOnly = false;
    bool isPrimary = false;
    uint accountId = 0;
    QString applicationId;
    QString providerName;

    Q_FOREACH(const QContactExtendedDetail &xDetail, contact.details<QContactExtendedDetail>()) {
        if (xDetail.name() == "READ-ONLY") {
            isReadOnly = xDetail.data().toBool();
        }
        if (xDetail.name() == "IS-PRIMARY") {
            isPrimary = xDetail.data().toBool();
        }
        if (xDetail.name() == "ACCOUNT-ID") {
            accountId = xDetail.data().toUInt();
        }
        if (xDetail.name() == "APPLICATION-ID") {
            applicationId = xDetail.data().toString();
        }
        if (xDetail.name() == "PROVIDER") {
            providerName = xDetail.data().toString();
        }
    }

    return Source(guid.guid(), displayLabel.label(), applicationId, providerName, accountId, isReadOnly, isPrimary);
}


} // namespace Galera



