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

#ifndef __GALERA_DIRTYCONTACT_NOTIFY_H__
#define __GALERA_DIRTYCONTACT_NOTIFY_H__

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QPointer>

namespace galera {

class AddressBookAdaptor;

// this is a helper class uses a timer with a small timeout to notify the client about
// any contact change notification. This class should be used instead of emit the signal directly
// this will avoid notify about the contact update several times when updating different fields simultaneously
// With that we can reduce the dbus traffic and skip some client calls to query about the new contact info.
class DirtyContactsNotify : public QObject
{
    Q_OBJECT

public:
    DirtyContactsNotify(AddressBookAdaptor *adaptor, QObject *parent=0);
    void insertChangedContacts(QSet<QString> ids);
    void insertRemovedContacts(QSet<QString> ids);
    void insertAddedContacts(QSet<QString> ids);
    void flush();
    void clear();

private Q_SLOTS:
    void emitSignals();

private:
    QPointer<AddressBookAdaptor> m_adaptor;
    QTimer m_timer;
    QSet<QString> m_contactsChanged;
    QSet<QString> m_contactsAdded;
    QSet<QString> m_contactsRemoved;
};


} //namespace

#endif
