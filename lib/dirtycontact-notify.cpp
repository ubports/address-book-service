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
    connect(&m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
}

void DirtyContactsNotify::append(QSet<QString> ids)
{
    m_ids += ids;
    m_timer.start();
}

void DirtyContactsNotify::onTimeout()
{
    Q_EMIT m_adaptor->contactsUpdated(m_ids.toList());
    m_ids.clear();
}

} //namespace
