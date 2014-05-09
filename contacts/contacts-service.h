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

#ifndef __CONTACTS_SERVICE_H__
#define __CONTACTS_SERVICE_H__

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtCore/QQueue>

#include <QtContacts/QContact>
#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactChangeSet>

#include <QtVersit/QVersitContactImporter>

#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>

class QDBusInterface;
using namespace QtContacts; // necessary for signal signatures

namespace galera {

class QContactRequestData;
class QContactSaveRequestData;
class QContactFetchRequestData;
class QContactRemoveRequestData;

class GaleraContactsService : public QObject
{
    Q_OBJECT
public:
    GaleraContactsService(const QString &managerUri);
    GaleraContactsService(const GaleraContactsService &other);
    ~GaleraContactsService();

    QList<QtContacts::QContactManagerEngine*> engines() const;
    void appendEngine(QtContacts::QContactManagerEngine *engine);
    void removeEngine(QtContacts::QContactManagerEngine *engine);

    QList<QtContacts::QContactRelationship> relationships() const;

    void addRequest(QtContacts::QContactAbstractRequest *request);
    void cancelRequest(QtContacts::QContactAbstractRequest *request);
    void waitRequest(QtContacts::QContactAbstractRequest *request);

Q_SIGNALS:
    void contactsAdded(QList<QContactId> ids);
    void contactsRemoved(QList<QContactId> ids);
    void contactsUpdated(QList<QContactId> ids);
    void serviceChanged();

private Q_SLOTS:
    void onContactsAdded(const QStringList &ids);
    void onContactsRemoved(const QStringList &ids);
    void onContactsUpdated(const QStringList &ids);
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onServiceReady();
    void onVCardsParsed(QList<QtContacts::QContact> contacts);

private:
    QString m_id;
    QtContacts::QContactId m_selfContactId;                     // the "MyCard" contact id
    QString m_managerUri;                                       // for faster lookup.
    QDBusServiceWatcher *m_serviceWatcher;
    bool m_serviceIsReady;
    int m_pageSize;

    QSharedPointer<QDBusInterface> m_iface;
    QString m_serviceName;
    QSet<QContactRequestData*> m_runningRequests;
    QQueue<QPointer<QtContacts::QContactAbstractRequest> > m_pendingRequests;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void deinitialize();

    bool isOnline() const;

    void fetchContacts(QtContacts::QContactFetchRequest *request);
    void fetchContactsContinue(QContactFetchRequestData *data,
                               QDBusPendingCallWatcher *call);
    void fetchContactsGroupsContinue(QContactFetchRequestData *request,
                                     QDBusPendingCallWatcher *call);
    void fetchContactsById(QtContacts::QContactFetchByIdRequest *request);
    void fetchContactsPage(QContactFetchRequestData *data);
    void fetchContactsDone(QContactFetchRequestData *data, QDBusPendingCallWatcher *call);

    void saveContact(QtContacts::QContactSaveRequest *request);
    void createContactsStart(QContactSaveRequestData *data);
    void updateContacts(QContactSaveRequestData *data);
    void updateContactDone(QContactSaveRequestData *data, QDBusPendingCallWatcher *call);
    void createContactsDone(QContactSaveRequestData *data, QDBusPendingCallWatcher *call);
    void createGroupDone(QContactSaveRequestData *data, QDBusPendingCallWatcher *call);

    void removeContact(QtContacts::QContactRemoveRequest *request);
    void removeContactDone(QContactRemoveRequestData *data, QDBusPendingCallWatcher *call);

    void destroyRequest(QContactRequestData *request);

    QList<QContactId> parseIds(const QStringList &ids) const;
};

}

#endif
