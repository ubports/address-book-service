/*
 * Copyright 2014 Canonical Ltd.
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


//this timeout represents how long the server will wait for changes on the contact before notify the client
#define NOTIFY_CONTACTS_TIMEOUT 500

#include "dirtycontact-notify.h"
#include "addressbook-adaptor.h"

namespace galera {

DirtyContactsNotify::DirtyContactsNotify(AddressBookAdaptor *adaptor, QObject *parent)
    : QObject(parent),
      m_adaptor(adaptor)
{
    m_timer.setInterval(NOTIFY_CONTACTS_TIMEOUT);
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()), SLOT(emitSignals()));
}

void DirtyContactsNotify::insertAddedContacts(QSet<QString> ids)
{
    if (!m_adaptor->isReady()) {
        return;
    }

    // if the contact was removed before ignore the removal signal, and send a update signal
    QSet<QString> addedIds = ids;
    Q_FOREACH(QString added, ids) {
        if (m_contactsRemoved.contains(added)) {
            m_contactsRemoved.remove(added);
            addedIds.remove(added);
            m_contactsChanged.insert(added);
        }
    }

    m_contactsAdded += addedIds;
    m_timer.start();
}

void DirtyContactsNotify::flush()
{
    m_timer.stop();
    emitSignals();
}

void DirtyContactsNotify::clear()
{
    m_timer.stop();
    m_contactsChanged.clear();
    m_contactsAdded.clear();
    m_contactsRemoved.clear();
}

void DirtyContactsNotify::insertRemovedContacts(QSet<QString> ids)
{
    if (!m_adaptor->isReady()) {
        return;
    }

    // if the contact was added before ignore the added and removed signal
    QSet<QString> removedIds = ids;
    Q_FOREACH(QString removed, ids) {
        if (m_contactsAdded.contains(removed)) {
            m_contactsAdded.remove(removed);
            removedIds.remove(removed);
        }
    }

    m_contactsRemoved += removedIds;
    m_timer.start();
}

void DirtyContactsNotify::insertChangedContacts(QSet<QString> ids)
{
    if (!m_adaptor->isReady()) {
        return;
    }

    m_contactsChanged += ids;
    m_timer.start();
}

void DirtyContactsNotify::emitSignals()
{
    if (!m_contactsChanged.isEmpty()) {
        // ignore the signal if the added signal was not fired yet
        m_contactsChanged.subtract(m_contactsAdded);
        // ignore the signal if the contact was removed
        m_contactsChanged.subtract(m_contactsRemoved);

        if (!m_contactsChanged.isEmpty()) {
            Q_EMIT m_adaptor->contactsUpdated(m_contactsChanged.toList());
            m_contactsChanged.clear();
        }
    }

    if (!m_contactsRemoved.isEmpty()) {
        Q_EMIT m_adaptor->contactsRemoved(m_contactsRemoved.toList());
        m_contactsRemoved.clear();
    }

    if (!m_contactsAdded.isEmpty()) {
        Q_EMIT m_adaptor->contactsAdded(m_contactsAdded.toList());
        m_contactsAdded.clear();
    }
}

} //namespace
