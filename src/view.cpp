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
#include "contacts-map.h"
#include "qindividual.h"

#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/dbus-service-defs.h"

#include <QtContacts/QContact>

#include <QtVersit/QVersitDocument>

using namespace QtContacts;
using namespace QtVersit;

namespace galera
{

class FilterThread: public QThread
{
public:
    FilterThread(QString filter, QString sort, ContactsMap *allContacts)
        : m_filter(filter),
          m_sort(sort),
          m_allContacts(allContacts)
    {
    }

    QList<ContactEntry*> result() const
    {
        if (isRunning()) {
            return QList<ContactEntry*>();
        } else {
            return m_contacts;
        }
    }

protected:
    void run()
    {
        Q_FOREACH(ContactEntry *entry, m_allContacts->values())
        {
            if (checkContact(entry)) {
                m_contacts << entry;
            }
        }
    }

private:
    ContactsMap *m_allContacts;
    QList<ContactEntry*> m_contacts;
    Filter m_filter;
    QString m_sort;

    bool checkContact(ContactEntry *entry)
    {
        //TODO: check query filter
        return m_filter.test(entry->individual()->contact());
    }
};

View::View(QString clause, QString sort, QStringList sources, ContactsMap *allContacts, QObject *parent)
    : QObject(parent),
      m_sources(sources),
      m_adaptor(0),
      m_filterThread(new FilterThread(clause, sort, allContacts))
{
    connect(m_filterThread, SIGNAL(finished()), this, SLOT(filterFinished()));
    m_filterThread->start();
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

    delete m_filterThread;
    m_filterThread = 0;
}

QString View::contactDetails(const QStringList &fields, const QString &id)
{
    return QString();
}

QStringList View::contactsDetails(const QStringList &fields, int startIndex, int pageSize)
{
    m_filterThread->wait();

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
    m_filterThread->wait();

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

void View::filterFinished()
{
    m_contacts = m_filterThread->result();
}

} //namespace
