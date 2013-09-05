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

#include "contacts-service.h"
#include "qcontact-engineid.h"
#include "request-data.h"

#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/fetch-hint.h"
#include "common/sort-clause.h"
#include "common/dbus-service-defs.h"

#include <QtCore/QSharedPointer>

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusConnectionInterface>

#include <QtContacts/QContactChangeSet>
#include <QtContacts/QContactName>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactFilter>

#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactImporter>
#include <QtVersit/QVersitContactExporter>
#include <QtVersit/QVersitWriter>

#define FETCH_PAGE_SIZE                 100

using namespace QtVersit;
using namespace QtContacts;

namespace galera
{

GaleraContactsService::GaleraContactsService(const QString &managerUri)
    : m_selfContactId(),
      m_managerUri(managerUri),
      m_serviceIsReady(false),
      m_iface(0)
{
    RequestData::registerMetaType();
    //qRegisterMetaType<galera::RequestData>();

    m_serviceWatcher = new QDBusServiceWatcher(CPIM_SERVICE_NAME,
                                               QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForOwnerChange,
                                               this);
    connect(m_serviceWatcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString,QString,QString)));

    initialize();
}

GaleraContactsService::GaleraContactsService(const GaleraContactsService &other)
    : m_selfContactId(other.m_selfContactId),
      m_managerUri(other.m_managerUri),
      m_iface(other.m_iface)
{
}

GaleraContactsService::~GaleraContactsService()
{
    while(!m_pendingRequests.isEmpty()) {
        QPointer<QContactAbstractRequest> request = m_pendingRequests.takeFirst();
        if (request) {
            QContactManagerEngine::updateRequestState(request,
                                                      QContactAbstractRequest::CanceledState);
        }
    }

    Q_ASSERT(m_runningRequests.size() == 0);
    delete m_serviceWatcher;
}

void GaleraContactsService::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);
    if (name == CPIM_SERVICE_NAME) {
        if (!newOwner.isEmpty()) {
            // service appear
            QMetaObject::invokeMethod(this, "initialize", Qt::QueuedConnection);
        } else if (!m_iface.isNull()) {
            // lost service
            QMetaObject::invokeMethod(this, "deinitialize", Qt::QueuedConnection);
        }
    }
}

void GaleraContactsService::onServiceReady()
{
    m_serviceIsReady = true;
    while(!m_pendingRequests.isEmpty()) {
        QPointer<QContactAbstractRequest> request = m_pendingRequests.takeFirst();
        if (request) {
            addRequest(request);
        }
    }
}

void GaleraContactsService::initialize()
{
    if (m_iface.isNull()) {
        m_iface = QSharedPointer<QDBusInterface>(new QDBusInterface(CPIM_SERVICE_NAME,
                                                                    CPIM_ADDRESSBOOK_OBJECT_PATH,
                                                                    CPIM_ADDRESSBOOK_IFACE_NAME));
        if (!m_iface->lastError().isValid()) {
            m_serviceIsReady = m_iface.data()->property("isReady").toBool();
            connect(m_iface.data(), SIGNAL(ready()), this, SLOT(onServiceReady()));
            connect(m_iface.data(), SIGNAL(contactsAdded(QStringList)), this, SLOT(onContactsAdded(QStringList)));
            connect(m_iface.data(), SIGNAL(contactsRemoved(QStringList)), this, SLOT(onContactsRemoved(QStringList)));
            connect(m_iface.data(), SIGNAL(contactsUpdated(QStringList)), this, SLOT(onContactsUpdated(QStringList)));
        } else {
            qWarning() << "Fail to connect with service:"  << m_iface->lastError();
            m_iface.clear();
        }
    }
}

void GaleraContactsService::deinitialize()
{
    if (!m_iface.isNull()) {
        m_id.clear();
        m_contacts.clear();
        m_contactIds.clear();
        m_relationships.clear();
        m_orderedRelationships.clear();
        m_iface.clear();
        Q_EMIT serviceChanged();
    }
}

bool GaleraContactsService::isOnline() const
{
    return !m_iface.isNull();
}

void GaleraContactsService::fetchContactsById(QtContacts::QContactFetchByIdRequest *request)
{
    qDebug() << Q_FUNC_INFO;

    if (!isOnline()) {
        RequestData::setError(request);
        return;
    }

    QContactIdFilter filter;
    filter.setIds(request->contactIds());
    QString filterStr = Filter(filter).toString();
    QDBusMessage result = m_iface->call("query", filterStr, "", QStringList());
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        RequestData::setError(request);
        return;
    }
    QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
    QDBusInterface *view = new QDBusInterface(CPIM_SERVICE_NAME,
                                             viewObjectPath.path(),
                                             CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

    RequestData *requestData = new RequestData(request, view, FetchHint());
    m_runningRequests << requestData;
    QMetaObject::invokeMethod(this, "fetchContactsPage", Qt::QueuedConnection, Q_ARG(galera::RequestData*, requestData));
}

void GaleraContactsService::fetchContacts(QtContacts::QContactFetchRequest *request)
{
    qDebug() << Q_FUNC_INFO;

    if (!isOnline()) {
        RequestData::setError(request);
        return;
    }
    QString sortStr = SortClause(request->sorting()).toString();
    QString filterStr = Filter(request->filter()).toString();
    FetchHint fetchHint = FetchHint(request->fetchHint()).toString();
    QDBusMessage result = m_iface->call("query", filterStr, sortStr, QStringList());
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        RequestData::setError(request);
        return;
    }
    QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
    QDBusInterface *view = new QDBusInterface(CPIM_SERVICE_NAME,
                                             viewObjectPath.path(),
                                             CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

    RequestData *requestData = new RequestData(request, view, fetchHint);

    m_runningRequests << requestData;
    QMetaObject::invokeMethod(this, "fetchContactsPage", Qt::QueuedConnection, Q_ARG(galera::RequestData*, requestData));
}

void GaleraContactsService::fetchContactsPage(RequestData *request)
{
    qDebug() << Q_FUNC_INFO;
    if (!isOnline() || !request->isLive()) {
        request->setError(QContactManager::UnspecifiedError);
        destroyRequest(request);
        return;
    }

    // Load contacs async
    QDBusPendingCall pcall = request->view()->asyncCall("contactsDetails", request->fields(), request->offset(), FETCH_PAGE_SIZE);
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        request->setError(QContactManager::UnspecifiedError);
        destroyRequest(request);
        return;
    }

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsDone(request, call);
                     });

    request->updateWatcher(watcher);
}

void GaleraContactsService::fetchContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;

    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QContactManager::Error opError = QContactManager::NoError;
    QContactAbstractRequest::State opState = QContactAbstractRequest::FinishedState;
    QDBusPendingReply<QStringList> reply = *call;
    QList<QContact> contacts;


    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QStringList vcards = reply.value();
        if (vcards.size() == FETCH_PAGE_SIZE) {
            opState = QContactAbstractRequest::ActiveState;
        }
        if (!vcards.isEmpty()) {
            contacts = VCardParser::vcardToContact(vcards);
            QList<QContactId> contactsIds;

            QList<QContact>::iterator contact;
            for (contact = contacts.begin(); contact != contacts.end(); ++contact) {
                if (!contact->isEmpty()) {
                    QContactGuid detailId = contact->detail<QContactGuid>();
                    GaleraEngineId *engineId = new GaleraEngineId(detailId.guid(), m_managerUri);
                    QContactId newId = QContactId(engineId);

                    contact->setId(newId);
                    contactsIds << newId;
                }
            }
            m_contacts += contacts;
            m_contactIds += contactsIds;
        }
    }

    request->update(contacts, opState, opError);

    if (opState == QContactAbstractRequest::ActiveState) {
        request->updateOffset(FETCH_PAGE_SIZE);
        request->updateWatcher(0);

        QMetaObject::invokeMethod(this, "fetchContactsPage", Qt::QueuedConnection, Q_ARG(galera::RequestData*, request));
    } else {
        destroyRequest(request);
    }
}

void GaleraContactsService::saveContact(QtContacts::QContactSaveRequest *request)
{
    qDebug() << Q_FUNC_INFO;
    QList<QContact> contacts = request->contacts();
    QStringList vcards = VCardParser::contactToVcard(contacts);

    if (vcards.size() != contacts.size()) {
        qWarning() << "Fail to convert contacts";
        return;
    }

    QStringList oldContacts;
    QStringList newContacts;

    for(int i=0, iMax=contacts.size(); i < iMax; i++) {
        if (contacts.at(i).id().isNull()) {
            newContacts << vcards[i];
        } else {
            oldContacts << vcards[i];
        }
    }

    if (!oldContacts.isEmpty()) {
        updateContacts(request, oldContacts);
    }

    if (!newContacts.isEmpty()) {
        createContacts(request, newContacts);
    }
}
void GaleraContactsService::createContacts(QtContacts::QContactSaveRequest *request, QStringList &contacts)
{
    qDebug() << Q_FUNC_INFO;
    if (!isOnline()) {
        RequestData::setError(request);
        return;
    }

    if (contacts.count() > 1) {
        qWarning() << "TODO: implement contact creation support to more then one contact.";
        return;
    }

    Q_FOREACH(QString contact, contacts) {
        QDBusPendingCall pcall = m_iface->asyncCall("createContact", contact, "");
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        RequestData *requestData = new RequestData(request, watcher);
        m_runningRequests << requestData;
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->createContactsDone(requestData, call);
                         });

    }
}

void GaleraContactsService::createContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;

    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QDBusPendingReply<QString> reply = *call;
    QList<QContact> contacts;
    QContactManager::Error opError = QContactManager::NoError;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QString id = reply.value();
        if (!id.isEmpty()) {
            contacts = static_cast<QtContacts::QContactSaveRequest *>(request->request())->contacts();
            GaleraEngineId *engineId = new GaleraEngineId(id, m_managerUri);
            QContactId contactId(engineId);
            contacts[0].setId(QContactId(contactId));
        } else {
            opError = QContactManager::UnspecifiedError;
        }
    }

    request->update(contacts, QContactAbstractRequest::FinishedState, opError);
    destroyRequest(request);
}

void GaleraContactsService::removeContact(QContactRemoveRequest *request)
{
    if (!isOnline()) {
        RequestData::setError(request);
        return;
    }

    QStringList ids;

    Q_FOREACH(QContactId contactId, request->contactIds()) {
        // TODO: find a better way to get the contactId
        ids << contactId.toString().split(":").last();
    }

    QDBusPendingCall pcall = m_iface->asyncCall("removeContacts", ids);
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        RequestData::setError(request);
    } else {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        RequestData *requestData = new RequestData(request, watcher);
        m_runningRequests << requestData;
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->removeContactDone(requestData, call);
                         });
    }
}

void GaleraContactsService::removeContactDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;

    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QDBusPendingReply<int> reply = *call;
    QContactManager::Error opError = QContactManager::NoError;
    QMap<int, QContactManager::Error> errorMap;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    }

    request->update(QContactAbstractRequest::FinishedState, opError);
    destroyRequest(request);
}

void GaleraContactsService::updateContacts(QtContacts::QContactSaveRequest *request, QStringList &contacts)
{
    qDebug() << Q_FUNC_INFO;
    if (!isOnline()) {
        RequestData::setError(request);
        return;
    }

    QDBusPendingCall pcall = m_iface->asyncCall("updateContacts", contacts);
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        RequestData::setError(request);
    } else {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        RequestData *requestData = new RequestData(request, watcher);
        m_runningRequests << requestData;
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->updateContactDone(requestData, call);
                         });
    }
}


void GaleraContactsService::updateContactDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;

    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QDBusPendingReply<QStringList> reply = *call;
    QList<QContact> contacts;
    QMap<int, QContactManager::Error> saveError;
    QContactManager::Error opError = QContactManager::NoError;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QStringList vcards = reply.value();
        if (!vcards.isEmpty()) {
            QMap<int, QVersitContactImporter::Error> importErrors;
            //TODO report parse errors
            contacts = VCardParser::vcardToContact(vcards);
            Q_FOREACH(int key, importErrors.keys()) {
                saveError.insert(key, QContactManager::BadArgumentError);
            }
        }
    }

    request->update(contacts, QContactAbstractRequest::FinishedState, opError, saveError);
    destroyRequest(request);
}

void GaleraContactsService::addRequest(QtContacts::QContactAbstractRequest *request)
{
    qDebug() << Q_FUNC_INFO << request->state();
    if (!m_serviceIsReady) {
        m_pendingRequests << QPointer<QtContacts::QContactAbstractRequest>(request);
        return;
    }
    if (!isOnline()) {
        QContactManagerEngine::updateRequestState(request, QContactAbstractRequest::FinishedState);
        return;
    }
    Q_ASSERT(request->state() == QContactAbstractRequest::ActiveState);
    switch (request->type()) {
        case QContactAbstractRequest::ContactFetchRequest:
            fetchContacts(static_cast<QContactFetchRequest*>(request));
            break;
        case QContactAbstractRequest::ContactFetchByIdRequest:
            fetchContactsById(static_cast<QContactFetchByIdRequest*>(request));
            break;
        case QContactAbstractRequest::ContactIdFetchRequest:
            qDebug() << "Not implemented: ContactIdFetchRequest";
            break;
        case QContactAbstractRequest::ContactSaveRequest:
            saveContact(static_cast<QContactSaveRequest*>(request));
            break;
        case QContactAbstractRequest::ContactRemoveRequest:
            removeContact(static_cast<QContactRemoveRequest*>(request));
            break;
        case QContactAbstractRequest::RelationshipFetchRequest:
            qDebug() << "Not implemented: RelationshipFetchRequest";
            break;
        case QContactAbstractRequest::RelationshipRemoveRequest:
            qDebug() << "Not implemented: RelationshipRemoveRequest";
            break;
        case QContactAbstractRequest::RelationshipSaveRequest:
            qDebug() << "Not implemented: RelationshipSaveRequest";
            break;
        break;

        default: // unknown request type.
        break;
    }
}

void GaleraContactsService::destroyRequest(RequestData *request)
{
    qDebug() << Q_FUNC_INFO;
    m_runningRequests.remove(request);
    delete request;
}

QList<QContactId> GaleraContactsService::parseIds(QStringList ids) const
{
    QList<QContactId> contactIds;
    Q_FOREACH(QString id, ids) {
        GaleraEngineId *engineId = new GaleraEngineId(id, m_managerUri);
        contactIds << QContactId(engineId);
    }
    return contactIds;
}


void GaleraContactsService::onContactsAdded(QStringList ids)
{
    Q_EMIT contactsAdded(parseIds(ids));
}

void GaleraContactsService::onContactsRemoved(QStringList ids)
{
    Q_EMIT contactsRemoved(parseIds(ids));
}

void GaleraContactsService::onContactsUpdated(QStringList ids)
{
    Q_EMIT contactsUpdated(parseIds(ids));
}

} //namespace
