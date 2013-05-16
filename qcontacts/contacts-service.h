/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * ontact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * webbrowser-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CONTACTS_SERVICE_H__
#define __CONTACTS_SERVICE_H__

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QAtomicInt>

#include <QtContacts/QContact>
#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactChangeSet>

#include <QtVersit/QVersitContactImporter>

#include <QtDBus/QDBusPendingCallWatcher>

class QDBusInterface;

namespace galera {

class RequestData;

class GaleraContactsService
{
public:
    GaleraContactsService(const QString &managerUri);
    GaleraContactsService(const GaleraContactsService &other);
    ~GaleraContactsService();

    QList<QtContacts::QContactManagerEngine*> engines() const;
    void appendEngine(QtContacts::QContactManagerEngine *engine);
    void removeEngine(QtContacts::QContactManagerEngine *engine);

    QList<QtContacts::QContact> contacts() const;
    QList<QtContacts::QContactId> contactIds() const;

    QList<QtContacts::QContactRelationship> relationships() const;

    void addRequest(QtContacts::QContactAbstractRequest *request);

private:
    QString m_id;
    QtContacts::QContactId m_selfContactId;                     // the "MyCard" contact id
    QList<QtContacts::QContact> m_contacts;                     // list of contacts
    QList<QtContacts::QContactId> m_contactIds;                 // list of contact Id's
    QList<QtContacts::QContactRelationship> m_relationships;    // list of contact relationships
    QMap<QtContacts::QContactId, QList<QtContacts::QContactRelationship> > m_orderedRelationships; // map of ordered lists of contact relationships
    QList<QString> m_definitionIds;                             // list of definition types (id's)
    quint32 m_nextContactId;
    QString m_managerUri;                                       // for faster lookup.

    QSharedPointer<QDBusInterface> m_iface;
    QSet<RequestData*> m_pendingRequests;

    void fetchContacts(QtContacts::QContactFetchRequest *request);
    void fetchContactsDone(RequestData *request, QDBusPendingCallWatcher *call);

    void saveContact(QtContacts::QContactSaveRequest *request);
    void createContacts(QtContacts::QContactSaveRequest *request, QStringList &contacts);
    void updateContacts(QtContacts::QContactSaveRequest *request, QStringList &contacts);
    void updateContactDone(RequestData *request, QDBusPendingCallWatcher *call);
    void createContactsDone(RequestData *request, QDBusPendingCallWatcher *call);

    void destroyRequest(RequestData *request);
    void fetchContactsPage(RequestData *request);
};

}

#endif
