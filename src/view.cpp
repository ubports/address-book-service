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

#include "view.h"
#include "view-adaptor.h"

#include "common/contacts-map.h"
#include "common/qindividual.h"
#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/dbus-service-defs.h"

#include <QtContacts/QContact>

#include <QtVersit/QVersitDocument>

using namespace QtContacts;
using namespace QtVersit;

namespace galera
{

View::View(QString clause, QString sort, QStringList sources, ContactsMap *allContacts, QObject *parent)
    : QObject(parent),
      m_filter(clause),
      m_sortClause(sort),
      m_sources(sources),
      m_adaptor(0),
      m_allContacts(allContacts)
{
    //TODO: run this async
    applyFilter();
}

View::~View()
{
    close();
}

void View::close()
{
    if (m_adaptor) {
        Q_EMIT m_adaptor->contactsRemoved(0, m_contacts.count());
        Q_EMIT closed();
        m_contacts.clear();

        QDBusConnection conn = QDBusConnection::sessionBus();
        unregisterObject(conn);
        m_adaptor->deleteLater();
        m_adaptor = 0;
    }
}

QString View::contactDetails(const QStringList &fields, const QString &id)
{
    return QString();
}

QStringList View::contactsDetails(const QStringList &fields, int startIndex, int pageSize)
{
    if (startIndex < 0) {
        startIndex = 0;
    }

    if ((pageSize < 0) || ((startIndex + pageSize) >= m_contacts.count())) {
        pageSize = m_contacts.count() - startIndex;
    }

    QList<QContact> contacts;
    for(int i = startIndex, iMax = (startIndex + pageSize); i < iMax; i++) {
        // TODO: filter fields
        contacts << m_contacts[i]->individual()->contact();
    }

    qDebug() << "Contacts details size:" << contacts.size();
    QStringList ret =  VCardParser::contactToVcard(contacts);
    qDebug() << "Parse result:" << contacts.size();
    return ret;
}

int View::count()
{
    return m_contacts.count();
}

void View::sort(const QString &field)
{
}

QString View::objectPath()
{
    return CPIM_ADDRESSBOOK_VIEW_OBJECT_PATH;
}

QString View::dynamicObjectPath() const
{
    return objectPath() + "/" + QString::number((long)this);
}

bool View::registerObject(QDBusConnection &connection)
{
    if (!m_adaptor) {
        m_adaptor = new ViewAdaptor(connection, this);
        if (!connection.registerObject(dynamicObjectPath(), this))
        {
            qWarning() << "Could not register object!" << objectPath();
            delete m_adaptor;
            m_adaptor = 0;
        } else {
            qDebug() << "Object registered:" << objectPath();
        }
    }

    return (m_adaptor != 0);
}

void View::unregisterObject(QDBusConnection &connection)
{
    if (m_adaptor) {
        connection.unregisterObject(dynamicObjectPath());
    }
}

QObject *View::adaptor() const
{
    return m_adaptor;
}

bool View::checkContact(ContactEntry *entry)
{
    //TODO: check query filter
    return m_filter.test(entry->individual()->contact());
}

bool View::appendContact(ContactEntry *entry)
{
    if (checkContact(entry)) {
        m_contacts << entry;
        return true;
    }
    return false;
}

void View::applyFilter()
{
    Q_FOREACH(ContactEntry *entry, m_allContacts->values())
    {
        appendContact(entry);
    }
    qSort(m_contacts.begin(), m_contacts.end(), m_sortClause);
}

} //namespace
