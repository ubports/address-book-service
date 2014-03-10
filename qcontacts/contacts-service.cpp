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

#define FETCH_PAGE_SIZE                 20

using namespace QtVersit;
using namespace QtContacts;

namespace galera
{
QElapsedTimer GaleraContactsService::m_speed;

GaleraContactsService::GaleraContactsService(const QString &managerUri)
    : m_selfContactId(),
      m_managerUri(managerUri),
      m_serviceIsReady(false),
      m_iface(0)
{
    RequestData::registerMetaType();

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
            request->cancel();
            request->waitForFinished();
        }
    }
    m_runningRequests.clear();

    delete m_serviceWatcher;
}

void GaleraContactsService::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);
    if (name == CPIM_SERVICE_NAME) {
        if (!newOwner.isEmpty()) {
            // service appear
            initialize();
        } else if (!m_iface.isNull()) {
            // lost service
            deinitialize();
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
    Q_FOREACH(RequestData* rData, m_runningRequests) {
        rData->cancel();
        rData->request()->waitForFinished();
        rData->setError(QContactManager::UnspecifiedError);
    }

    if (!m_iface.isNull()) {
        m_id.clear();
        Q_EMIT serviceChanged();
    }

    // this will make the service re-initialize
    QDBusMessage result = m_iface->call("ping");
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        m_serviceIsReady = false;
    } else {
        m_serviceIsReady = m_iface.data()->property("isReady").toBool();
    }
}

bool GaleraContactsService::isOnline() const
{
    return !m_iface.isNull();
}

void GaleraContactsService::fetchContactsById(QtContacts::QContactFetchByIdRequest *request)
{
    if (!isOnline()) {
        qWarning() << "Server is not online";
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
    m_speed.restart();
    qDebug() << "START FETCH CONTACTS[BEGIN]" << m_speed.elapsed();
    if (!isOnline()) {
        qWarning() << "Server is not online";
        RequestData::setError(request);
        return;
    }
    QString sortStr = SortClause(request->sorting()).toString();
    QString filterStr = Filter(request->filter()).toString();
    FetchHint fetchHint = FetchHint(request->fetchHint()).toString();

    QDBusPendingCall pcall = m_iface->asyncCall("query", filterStr, sortStr, QStringList());
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        RequestData::setError(request);
        return;
    }

    RequestData *requestData = new RequestData(request, 0, fetchHint);
    m_runningRequests << requestData;

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsContinue(requestData, call);
                     });
    qDebug() << "START FETCH CONTACTS[END]" << m_speed.elapsed();
}


void GaleraContactsService::fetchContactsContinue(RequestData *request,
                                                  QDBusPendingCallWatcher *call)
{
    qDebug() << "fetchContactsContinue[BEGIN]" << m_speed.elapsed();
    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QDBusPendingReply<QDBusObjectPath> reply = *call;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        destroyRequest(request);
    } else {
        QDBusObjectPath viewObjectPath = reply.value();
        QDBusInterface *view = new QDBusInterface(CPIM_SERVICE_NAME,
                                                  viewObjectPath.path(),
                                                  CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);
        request->updateView(view);
        qDebug() << "WILL FETCH PAGE";
        QMetaObject::invokeMethod(this, "fetchContactsPage", Qt::QueuedConnection, Q_ARG(galera::RequestData*, request));
    }
    qDebug() << "fetchContactsContinue[END]" << m_speed.elapsed();
}

void GaleraContactsService::fetchContactsPage(RequestData *request)
{
    qDebug() << "fetchContactsPage[BEGIN]" << m_speed.elapsed();
    if (!isOnline() || !request->isLive()) {
        qWarning() << "Server is not online";
        destroyRequest(request);
        return;
    }

    // Load contacs async
    QDBusPendingCall pcall = request->view()->asyncCall("contactsDetails",
                                                        request->fields(),
                                                        request->offset(),
                                                        FETCH_PAGE_SIZE);
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        request->setError(QContactManager::UnspecifiedError);
        destroyRequest(request);
        return;
    }

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
    request->updateWatcher(watcher);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsDone(request, call);
                     });
    qDebug() << "fetchContactsPage[END]" << m_speed.elapsed();
}

void GaleraContactsService::fetchContactsDone(RequestData *request, QDBusPendingCallWatcher *call)
{
    qDebug() << "fetchContactsDone[BEGIN]" << m_speed.elapsed();
    if (!request->isLive()) {
        destroyRequest(request);
        return;
    }

    QDBusPendingReply<QStringList> reply = *call;
    qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
    if (reply.isError()) {
        qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
        qWarning() << reply.error().name() << reply.error().message();

        request->update(QList<QContact>(),
                        QContactAbstractRequest::FinishedState,
                        QContactManager::UnspecifiedError);
        destroyRequest(request);
    } else {
        qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
        const QStringList vcards = reply.value();
        qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
        qDebug() << "VCARDS SIZE:" << vcards.size();
        if (vcards.size()) {
            VCardParser *parser = new VCardParser(this);
            parser->setProperty("DATA", QVariant::fromValue<void*>(request));
            connect(parser, &VCardParser::contactsParsed,
                    this, &GaleraContactsService::onVCardsParsed);
            parser->vcardToContact(vcards);
        } else {
            qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
            request->update(QList<QContact>(), QContactAbstractRequest::FinishedState);
            destroyRequest(request);
            qDebug() << "fetchContactsDone[" << __LINE__ << "]" << m_speed.elapsed();
        }
    }
    qDebug() << "fetchContactsDone[END]" << m_speed.elapsed();
}

void GaleraContactsService::onVCardsParsed(QList<QContact> contacts)
{
    qDebug() << "onVCardsParsed[BEGIN]" << m_speed.elapsed();
    QObject *sender = QObject::sender();
    RequestData *request = static_cast<RequestData*>(sender->property("DATA").value<void*>());

    QList<QContact>::iterator contact;
    for (contact = contacts.begin(); contact != contacts.end(); ++contact) {
        if (!contact->isEmpty()) {
            QContactGuid detailId = contact->detail<QContactGuid>();
            GaleraEngineId *engineId = new GaleraEngineId(detailId.guid(), m_managerUri);
            QContactId newId = QContactId(engineId);
            contact->setId(newId);
            // set tag to be used when creating sections
            QContactName detailName = contact->detail<QContactName>();
            if (!detailName.firstName().isEmpty() && QString(detailName.firstName().at(0)).contains(QRegExp("([a-z]|[A-Z])"))) {
                contact->addTag(detailName.firstName().at(0).toUpper());
            } else if (!detailName.lastName().isEmpty() && QString(detailName.lastName().at(0)).contains(QRegExp("([a-z]|[A-Z])"))) {
                contact->addTag(detailName.lastName().at(0).toUpper());
            } else {
                contact->addTag("#");
            }
        }
    }

    if (contacts.size() == FETCH_PAGE_SIZE) {
        request->update(contacts, QContactAbstractRequest::ActiveState);
        request->updateOffset(FETCH_PAGE_SIZE);
        request->updateWatcher(0);
        QMetaObject::invokeMethod(this, "fetchContactsPage", Qt::QueuedConnection, Q_ARG(galera::RequestData*, request));
    } else {
        request->update(contacts, QContactAbstractRequest::FinishedState);
        destroyRequest(request);
    }

    sender->deleteLater();
    qDebug() << "onVCardsParsed[END]" << m_speed.elapsed();
}

void GaleraContactsService::saveContact(QtContacts::QContactSaveRequest *request)
{
    QList<QContact> contacts = request->contacts();
    QStringList oldContacts;
    QStringList newContacts;

    Q_FOREACH(const QContact &contact, contacts) {
        if (contact.id().isNull()) {
            newContacts << VCardParser::contactToVcard(contact);
        } else {
            oldContacts << VCardParser::contactToVcard(contact);
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
    if (!isOnline()) {
        qWarning() << "Server is not online";
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
        qWarning() << "Server is not online";
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
    if (!isOnline()) {
        qWarning() << "Server is not online";
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
            contacts = VCardParser::vcardToContactSync(vcards);
            Q_FOREACH(int key, importErrors.keys()) {
                saveError.insert(key, QContactManager::BadArgumentError);
            }
        }
    }

    request->update(contacts, QContactAbstractRequest::FinishedState, opError, saveError);
    destroyRequest(request);
}

void GaleraContactsService::cancelRequest(QtContacts::QContactAbstractRequest *request)
{
    Q_FOREACH(RequestData* rData, m_runningRequests) {
        if (rData->request() == request) {
            rData->cancel();
            return;
        }
    }
}

void GaleraContactsService::waitRequest(QtContacts::QContactAbstractRequest *request)
{
    Q_FOREACH(RequestData* rData, m_runningRequests) {
        if (rData->request() == request) {
            rData->wait();
            return;
        }
    }
}

void GaleraContactsService::addRequest(QtContacts::QContactAbstractRequest *request)
{
    if (!m_serviceIsReady) {
        m_pendingRequests << QPointer<QtContacts::QContactAbstractRequest>(request);
        return;
    }

    if (!isOnline()) {
        qWarning() << "Server is not online";
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
