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

#include "addressbook-adaptor.h"
#include "addressbook.h"
#include "view.h"

namespace galera
{

AddressBookAdaptor::AddressBookAdaptor(const QDBusConnection &connection, AddressBook *parent)
    : QDBusAbstractAdaptor(parent),
      m_addressBook(parent),
      m_connection(connection)
{
    setAutoRelaySignals(true);
    connect(m_addressBook, SIGNAL(readyChanged()), SIGNAL(readyChanged()));
    connect(m_addressBook, SIGNAL(safeModeChanged()), SIGNAL(safeModeChanged()));
}

AddressBookAdaptor::~AddressBookAdaptor()
{
    // destructor
}

SourceList AddressBookAdaptor::availableSources(const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "availableSources",
                              Qt::QueuedConnection,
                              Q_ARG(const QDBusMessage&, message));
    return SourceList();
}

Source AddressBookAdaptor::source(const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "source",
                              Qt::QueuedConnection,
                              Q_ARG(const QDBusMessage&, message));
    return Source();
}

Source AddressBookAdaptor::createSource(const QString &sourceName, bool setAsPrimary, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "createSource",
                              Qt::QueuedConnection,
                              Q_ARG(const QString&, sourceName),
                              Q_ARG(uint, 0),
                              Q_ARG(bool, setAsPrimary),
                              Q_ARG(const QDBusMessage&, message));
    return Source();
}

Source AddressBookAdaptor::createSourceForAccount(const QString &sourceName,
                                                  uint accountId,
                                                  bool setAsPrimary,
                                                  const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "createSource",
                              Qt::QueuedConnection,
                              Q_ARG(const QString&, sourceName),
                              Q_ARG(uint, accountId),
                              Q_ARG(bool, setAsPrimary),
                              Q_ARG(const QDBusMessage&, message));
    return Source();
}

SourceList AddressBookAdaptor::updateSources(const SourceList &sources, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "updateSources",
                              Qt::QueuedConnection,
                              Q_ARG(const SourceList&, sources),
                              Q_ARG(const QDBusMessage&, message));
}

bool AddressBookAdaptor::removeSource(const QString &sourceId, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "removeSource",
                              Qt::QueuedConnection,
                              Q_ARG(const QString&, sourceId),
                              Q_ARG(const QDBusMessage&, message));
    return false;
}


QString AddressBookAdaptor::createContact(const QString &contact, const QString &source, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "createContact",
                              Qt::QueuedConnection,
                              Q_ARG(const QString&, contact),
                              Q_ARG(const QString&, source),
                              Q_ARG(const QDBusMessage&, message));
    return QString();
}

QDBusObjectPath AddressBookAdaptor::query(const QString &clause, const QString &sort, int maxCount, bool showInvisible, const QStringList &sources)
{
    View *v = m_addressBook->query(clause, sort, maxCount, showInvisible, sources);
    v->registerObject(m_connection);
    return QDBusObjectPath(v->dynamicObjectPath());
}

int AddressBookAdaptor::removeContacts(const QStringList &contactIds, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "removeContacts",
                              Qt::QueuedConnection,
                              Q_ARG(const QStringList&, contactIds),
                              Q_ARG(const QDBusMessage&, message));
    return 0;
}

QStringList AddressBookAdaptor::sortFields()
{
    return m_addressBook->sortFields();
}

QString AddressBookAdaptor::linkContacts(const QStringList &contactsIds)
{
    return m_addressBook->linkContacts(contactsIds);
}

bool AddressBookAdaptor::unlinkContacts(const QString &parentId, const QStringList &contactsIds)
{
    return m_addressBook->unlinkContacts(parentId, contactsIds);
}

QStringList AddressBookAdaptor::updateContacts(const QStringList &contacts, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    QMetaObject::invokeMethod(m_addressBook, "updateContacts",
                              Qt::QueuedConnection,
                              Q_ARG(const QStringList&, contacts),
                              Q_ARG(const QDBusMessage&, message));
    return QStringList();
}

bool AddressBookAdaptor::isReady()
{
    return m_addressBook->isReady();
}

bool AddressBookAdaptor::ping()
{
    return true;
}

void AddressBookAdaptor::purgeContacts(const QString &since, const QString &sourceId, const QDBusMessage &message)
{
    QDateTime sinceDate;
    if (since.isEmpty()) {
        sinceDate = QDateTime::fromTime_t(0);
    } else {
        sinceDate = QDateTime::fromString(since, Qt::ISODate);
    }
    m_addressBook->purgeContacts(sinceDate, sourceId, message);
}

void AddressBookAdaptor::shutDown() const
{
    m_addressBook->shutdown();
}

bool AddressBookAdaptor::safeMode() const
{
    return m_addressBook->isSafeMode();
}

void AddressBookAdaptor::setSafeMode(bool flag)
{
    m_addressBook->setSafeMode(flag);
}

} //namespace
