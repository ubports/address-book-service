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
#include "contact-less-than.h"
#include "qindividual.h"

#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/fetch-hint.h"
#include "common/dbus-service-defs.h"

#include <QtContacts/QContact>

#include <QtVersit/QVersitDocument>

#include <QtCore/QReadWriteLock>
#include <QtCore/QCoreApplication>

using namespace QtContacts;
using namespace QtVersit;

namespace galera
{

class FilterThread: public QThread
{
public:
    FilterThread(QString filter, QString sort, ContactsMap *allContacts)
        : m_filter(filter),
          m_sortClause(sort),
          m_allContacts(allContacts),
          m_stopped(false)
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

    bool appendContact(ContactEntry *entry)
    {
        if (checkContact(entry)) {
            addSorted(&m_contacts, entry, m_sortClause);
            return true;
        }
        return false;
    }

    bool removeContact(ContactEntry *entry)
    {
        return m_contacts.removeAll(entry);
    }

    void chageSort(SortClause clause)
    {
        m_sortClause = clause;
        if (!clause.isEmpty()) {
            ContactLessThan lessThan(m_sortClause);
            qSort(m_contacts.begin(), m_contacts.end(), lessThan);
        }
    }

    void addSorted(QList<ContactEntry*>* sorted, ContactEntry* toAdd, const SortClause& sortOrder)
    {
        if (!sortOrder.isEmpty()) {
            ContactLessThan lessThan(sortOrder);
            QList<ContactEntry*>::iterator it(std::upper_bound(sorted->begin(), sorted->end(), toAdd, lessThan));
            sorted->insert(it, toAdd);
        } else {
            // no sort order? just add it to the end
            sorted->append(toAdd);
        }
    }

    void stop()
    {
        m_stoppedLock.lockForWrite();
        m_stopped = true;
        m_stoppedLock.unlock();
    }

protected:
    void run()
    {
        m_allContacts->lock();

        // filter contacts if necessary
        if (m_filter.isValid()) {
            Q_FOREACH(ContactEntry *entry, m_allContacts->values())
            {
                m_stoppedLock.lockForRead();
                if (m_stopped) {
                    m_stoppedLock.unlock();
                    m_allContacts->unlock();
                    return;
                }
                m_stoppedLock.unlock();

                if (checkContact(entry)) {
                    addSorted(&m_contacts, entry, m_sortClause);
                }
            }
        } else {
            m_contacts = m_allContacts->values();
            chageSort(m_sortClause);
        }

        m_allContacts->unlock();
    }

private:
    Filter m_filter;
    SortClause m_sortClause;
    ContactsMap *m_allContacts;
    QList<ContactEntry*> m_contacts;
    bool m_stopped;
    QReadWriteLock m_stoppedLock;

    bool checkContact(ContactEntry *entry)
    {
        return m_filter.test(entry->individual()->contact());
    }
};

View::View(const QString &clause, const QString &sort, const QStringList &sources, ContactsMap *allContacts, QObject *parent)
    : QObject(parent),
      m_sources(sources),
      m_filterThread(new FilterThread(clause, sort, allContacts)),
      m_adaptor(0)
{
    m_filterThread->start();
    connect(m_filterThread, SIGNAL(finished()), SIGNAL(countChanged()));
}

View::~View()
{
    close();
}

void View::close()
{
    if (m_adaptor) {
        Q_EMIT m_adaptor->contactsRemoved(0, m_filterThread->result().count());
        Q_EMIT closed();

        QDBusConnection conn = QDBusConnection::sessionBus();
        unregisterObject(conn);
        m_adaptor->deleteLater();
        m_adaptor = 0;
    }

    if (m_filterThread) {
        if (m_filterThread->isRunning()) {
            m_filterThread->stop();
            m_filterThread->wait();
        }
        delete m_filterThread;
        m_filterThread = 0;
    }
}

QString View::contactDetails(const QStringList &fields, const QString &id)
{
    Q_ASSERT(FALSE);
    return QString();
}

QStringList View::contactsDetails(const QStringList &fields, int startIndex, int pageSize, const QDBusMessage &message)
{
    while(!m_filterThread->wait(300)) {
        QCoreApplication::processEvents();
    }

    QList<ContactEntry*> entries = m_filterThread->result();
    if (startIndex < 0) {
        startIndex = 0;
    }

    if ((pageSize < 0) || ((startIndex + pageSize) >= entries.count())) {
        pageSize = entries.count() - startIndex;
    }

    QList<QContact> contacts;
    for(int i = startIndex, iMax = (startIndex + pageSize); i < iMax; i++) {
        contacts << entries[i]->individual()->copy(FetchHint::parseFieldNames(fields));
    }

    VCardParser *parser = new VCardParser(this);
    parser->setProperty("DATA", QVariant::fromValue<QDBusMessage>(message));
    connect(parser, &VCardParser::vcardParsed,
            this, &View::onVCardParsed);
    parser->contactToVcard(contacts);
    return QStringList();
}

void View::onVCardParsed(const QStringList &vcards)
{
    QObject *sender = QObject::sender();
    QDBusMessage reply = sender->property("DATA").value<QDBusMessage>().createReply(vcards);
    QDBusConnection::sessionBus().send(reply);
    sender->deleteLater();
}

int View::count()
{
    m_filterThread->wait();

    return m_filterThread->result().count();
}

void View::sort(const QString &field)
{
    m_filterThread->chageSort(SortClause(field));
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
            connect(this, SIGNAL(countChanged(int)), m_adaptor, SIGNAL(countChanged(int)));
        }
    }

    return (m_adaptor != 0);
}

void View::unregisterObject(QDBusConnection &connection)
{
    if (m_adaptor) {
        qDebug() << "Object UN-registered:" << objectPath();
        connection.unregisterObject(dynamicObjectPath());
    }
}

bool View::appendContact(ContactEntry *entry)
{
    if (m_filterThread->appendContact(entry)) {
        Q_EMIT countChanged(m_filterThread->result().count());
        return true;
    }
    return false;
}

bool View::removeContact(ContactEntry *entry)
{
    if (m_filterThread->removeContact(entry)) {
        Q_EMIT countChanged(m_filterThread->result().count());
        return true;
    }
    return false;
}

QObject *View::adaptor() const
{
    return m_adaptor;
}

} //namespace
