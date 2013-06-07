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
#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/dbus-service-defs.h"

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

#include <QtContacts/QContactChangeSet>
#include <QtContacts/QContactName>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactFilter>

#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactImporter>
#include <QtVersit/QVersitContactExporter>
#include <QtVersit/QVersitWriter>

#define GALERA_SERVICE_NAME             "com.canonical.galera"
#define GALERA_ADDRESSBOOK_OBJECT_PATH  "/com/canonical/galera/AddressBook"
#define GALERA_ADDRESSBOOK_IFACE_NAME   "com.canonical.galera.AddressBook"
#define GALERA_VIEW_IFACE_NAME          "com.canonical.galera.View"

#define FETCH_PAGE_SIZE                 30

using namespace QtVersit;

namespace galera
{

class RequestData
{
public:
    QtContacts::QContactAbstractRequest *m_request;
    QDBusInterface *m_view;
    QDBusPendingCallWatcher *m_watcher;
    QList<QContact> m_result;
    int m_offset;

    RequestData()
        : m_request(0), m_view(0), m_watcher(0), m_offset(0)
    {}

    ~RequestData()
    {
        m_request = 0;
        if (m_watcher) {
            m_watcher->deleteLater();
            m_watcher = 0;
        }
        if (m_view) {
            QDBusMessage ret = m_view->call("close");
            m_view->deleteLater();
            m_view = 0;
        }
    }
};

GaleraContactsService::GaleraContactsService(const QString &managerUri)
    : m_selfContactId(),
      m_nextContactId(1),
      m_managerUri(managerUri)

{
    m_iface = QSharedPointer<QDBusInterface>(new QDBusInterface(CPIM_SERVICE_NAME,
                                                                CPIM_ADDRESSBOOK_OBJECT_PATH,
                                                                CPIM_ADDRESSBOOK_IFACE_NAME));
    connect(m_iface.data(), SIGNAL(contactsAdded(QStringList)), this, SLOT(onContactsAdded(QStringList)));
    connect(m_iface.data(), SIGNAL(contactsRemoved(QStringList)), this, SLOT(onContactsRemoved(QStringList)));
}

GaleraContactsService::GaleraContactsService(const GaleraContactsService &other)
    : m_selfContactId(other.m_selfContactId),
      m_nextContactId(other.m_nextContactId),
      m_iface(other.m_iface),
      m_managerUri(other.m_managerUri)
{
}

GaleraContactsService::~GaleraContactsService()
{
}

void GaleraContactsService::fetchContacts(QtContacts::QContactFetchRequest *request)
{
    qDebug() << Q_FUNC_INFO;
    //QContactFetchRequest *r = static_cast<QContactFetchRequest*>(request);
    //QContactFilter filter = r->filter();
    //QList<QContactSortOrder> sorting = r->sorting();
    //QContactFetchHint fetchHint = r->fetchHint();
    //QContactManager::Error operationError = QContactManager::NoError;

    QString filterStr = Filter(request->filter()).toString();
    QDBusMessage result = m_iface->call("query", filterStr, "", QStringList());
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        QContactManagerEngine::updateContactFetchRequest(request, QList<QContact>(),
                                                         QContactManager::UnspecifiedError,
                                                         QContactAbstractRequest::FinishedState);
        return;
    }
    RequestData *requestData = new RequestData;
    QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
    requestData->m_view = new QDBusInterface(CPIM_SERVICE_NAME,
                                             viewObjectPath.path(),
                                             CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

    requestData->m_request = request;
    m_pendingRequests << requestData;
    fetchContactsPage(requestData);
}

void GaleraContactsService::fetchContactsPage(RequestData *request)
{
    qDebug() << Q_FUNC_INFO;
    // Load contacs async
    QDBusPendingCall pcall = request->m_view->asyncCall("contactsDetails", QStringList(), request->m_offset, FETCH_PAGE_SIZE);
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(request->m_request), QList<QContact>(),
                                                         QContactManager::UnspecifiedError,
                                                         QContactAbstractRequest::FinishedState);
        destroyRequest(request);
        return;
    }
    request->m_watcher = new QDBusPendingCallWatcher(pcall, 0);
    QObject::connect(request->m_watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsDone(request, call);
                     });
}

void GaleraContactsService::fetchContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;
    QContactManager::Error opError = QContactManager::NoError;
    QContactAbstractRequest::State opState = QContactAbstractRequest::FinishedState;
    QDBusPendingReply<QStringList> reply = *call;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QStringList vcards = reply.value();
        if (vcards.size() == FETCH_PAGE_SIZE) {
            opState = QContactAbstractRequest::ActiveState;
        }

        if (!vcards.isEmpty()) {
            QList<QContact> contacts = VCardParser::vcardToContact(vcards);
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
            request->m_result += contacts;
        }
    }
    QContactManagerEngine::updateContactFetchRequest(static_cast<QContactFetchRequest*>(request->m_request),
                                                     request->m_result,
                                                     opError,
                                                     opState);

    if (opState == QContactAbstractRequest::ActiveState) {
        request->m_offset += FETCH_PAGE_SIZE;
        request->m_watcher->deleteLater();
        request->m_watcher = 0;
        fetchContactsPage(request);
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
    if (contacts.count() > 1) {
        qWarning() << "TODO: implement contact creation support to more then one contact.";
        return;
    }

    Q_FOREACH(QString contact, contacts) {
        QDBusPendingCall pcall = m_iface->asyncCall("createContact", contact, "");
        RequestData *requestData = new RequestData;
        requestData->m_request = request;
        requestData->m_watcher = new QDBusPendingCallWatcher(pcall, 0);
        QObject::connect(requestData->m_watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->createContactsDone(requestData, call);
                         });
    }
}

void GaleraContactsService::createContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;
    QDBusPendingReply<QString> reply = *call;
    QList<QContact> contacts;
    QMap<int, QContactManager::Error> errorMap;
    QContactManager::Error opError = QContactManager::NoError;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QString id = reply.value();
        if (!id.isEmpty()) {
            contacts = static_cast<QtContacts::QContactSaveRequest *>(request->m_request)->contacts();
            GaleraEngineId *engineId = new GaleraEngineId(id, m_managerUri);
            QContactId contactId(engineId);
            contacts[0].setId(QContactId(contactId));
        } else {
            opError = QContactManager::UnspecifiedError;
        }
    }

    if (opError != QContactManager::NoError) {
        QContactManagerEngine::updateContactSaveRequest(static_cast<QContactSaveRequest*>(request->m_request),
                                                        contacts,
                                                        opError,
                                                        errorMap,
                                                        QContactAbstractRequest::FinishedState);
    } else {
        QContactManagerEngine::updateRequestState(request->m_request,
                                                  QContactAbstractRequest::FinishedState);
    }
    destroyRequest(request);
}

void GaleraContactsService::removeContact(QContactRemoveRequest *request)
{
    QStringList ids;

    Q_FOREACH(QContactId contactId, request->contactIds()) {
        // TODO: find a better way to get the contactId
        ids << contactId.toString().split(":").last();
    }

    QDBusPendingCall pcall = m_iface->asyncCall("removeContacts", ids);
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        QContactManagerEngine::updateContactRemoveRequest(request,
                                                          QContactManager::UnspecifiedError,
                                                          QMap<int, QContactManager::Error>(),
                                                          QContactAbstractRequest::FinishedState);
    } else {
        RequestData *requestData = new RequestData;
        requestData->m_request = request;
        requestData->m_watcher = new QDBusPendingCallWatcher(pcall, 0);
        QObject::connect(requestData->m_watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->removeContactDone(requestData, call);
                         });
    }
}

void GaleraContactsService::removeContactDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;
    QDBusPendingReply<int> reply = *call;
    QContactManager::Error opError = QContactManager::NoError;
    QMap<int, QContactManager::Error> errorMap;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    }


    if (opError != QContactManager::NoError) {
        QContactManagerEngine::updateContactRemoveRequest(static_cast<QContactRemoveRequest*>(request->m_request),
                                                          opError,
                                                          errorMap,
                                                          QContactAbstractRequest::FinishedState);
    } else {
        QContactManagerEngine::updateRequestState(request->m_request, QContactAbstractRequest::FinishedState);
    }

    destroyRequest(request);
}

void GaleraContactsService::updateContacts(QtContacts::QContactSaveRequest *request, QStringList &contacts)
{
    qDebug() << Q_FUNC_INFO;
    QDBusPendingCall pcall = m_iface->asyncCall("updateContacts", contacts);
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        QContactManagerEngine::updateContactSaveRequest(request,
                                                        QList<QContact>(),
                                                        QContactManager::UnspecifiedError,
                                                        QMap<int, QContactManager::Error>(),
                                                        QContactAbstractRequest::FinishedState);
    } else {
        RequestData *requestData = new RequestData;
        requestData->m_request = request;
        requestData->m_watcher = new QDBusPendingCallWatcher(pcall, 0);
        QObject::connect(requestData->m_watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->updateContactDone(requestData, call);
                         });
    }
}


void GaleraContactsService::updateContactDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << Q_FUNC_INFO;
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

    if (opError != QContactManager::NoError) {
        QContactManagerEngine::updateContactSaveRequest(static_cast<QContactSaveRequest*>(request->m_request),
                                                        contacts,
                                                        opError,
                                                        saveError,
                                                        QContactAbstractRequest::FinishedState);
    } else {
        QContactManagerEngine::updateRequestState(request->m_request,
                                                  QContactAbstractRequest::FinishedState);
    }
    destroyRequest(request);
}

void GaleraContactsService::addRequest(QtContacts::QContactAbstractRequest *request)
{
    qDebug() << Q_FUNC_INFO << request->state();
    Q_ASSERT(request->state() == QContactAbstractRequest::ActiveState);
    switch (request->type()) {
        case QContactAbstractRequest::ContactFetchRequest:
            fetchContacts(static_cast<QContactFetchRequest*>(request));
            break;
        case QContactAbstractRequest::ContactFetchByIdRequest:
            qDebug() << "Not implemented: ContactFetchByIdRequest";
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
    m_pendingRequests.remove(request);
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

} //namespace
