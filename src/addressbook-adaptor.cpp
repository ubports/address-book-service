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
}

AddressBookAdaptor::~AddressBookAdaptor()
{
    // destructor
}

SourceList AddressBookAdaptor::availableSources()
{
    return m_addressBook->availableSources();
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

QDBusObjectPath AddressBookAdaptor::query(const QString &clause, const QString &sort, const QStringList &sources)
{
    View *v = m_addressBook->query(clause, sort, sources);
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

} //namespace
