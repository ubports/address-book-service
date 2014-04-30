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
#include "qcontactrequest-data.h"
#include "qcontactfetchrequest-data.h"
#include "qcontactfetchbyidrequest-data.h"
#include "qcontactremoverequest-data.h"
#include "qcontactsaverequest-data.h"

#include "common/vcard-parser.h"
#include "common/filter.h"
#include "common/fetch-hint.h"
#include "common/sort-clause.h"
#include "common/dbus-service-defs.h"
#include "common/source.h"

#include <QtCore/QSharedPointer>

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusConnectionInterface>

#include <QtContacts/QContact>
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

namespace //private
{

static QContact parseSource(const galera::Source &source, const QString &managerUri)
{
    QContact contact;

    // contact group type
    contact.setType(QContactType::TypeGroup);

    // id
    galera::GaleraEngineId *engineId = new galera::GaleraEngineId(source.id(), managerUri);
    QContactId newId = QContactId(engineId);
    contact.setId(newId);

    // guid
    QContactGuid guid;
    guid.setGuid(source.id());
    contact.saveDetail(&guid);

    // display name
    QContactDisplayLabel displayLabel;
    displayLabel.setLabel(source.displayLabel());
    contact.saveDetail(&displayLabel);

    // read-only
    QContactExtendedDetail readOnly;
    readOnly.setName("READ-ONLY");
    readOnly.setData(source.isReadOnly());
    contact.saveDetail(&readOnly);

    // Primary
    QContactExtendedDetail primary;
    primary.setName("IS-PRIMARY");
    primary.setData(source.isPrimary());
    contact.saveDetail(&primary);

    return contact;
}

}

namespace galera
{
GaleraContactsService::GaleraContactsService(const QString &managerUri)
    : m_selfContactId(),
      m_managerUri(managerUri),
      m_serviceIsReady(false),
      m_iface(0)
{
    Source::registerMetaType();

    if (qEnvironmentVariableIsSet(ALTERNATIVE_CPIM_SERVICE_NAME)) {
        m_serviceName = qgetenv(ALTERNATIVE_CPIM_SERVICE_NAME);
    } else {
        m_serviceName = CPIM_SERVICE_NAME;
    }

    m_serviceWatcher = new QDBusServiceWatcher(m_serviceName,
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
    if (name == m_serviceName) {
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
        m_iface = QSharedPointer<QDBusInterface>(new QDBusInterface(m_serviceName,
                                                                    CPIM_ADDRESSBOOK_OBJECT_PATH,
                                                                    CPIM_ADDRESSBOOK_IFACE_NAME));
        if (!m_iface->lastError().isValid()) {
            m_serviceIsReady = m_iface.data()->property("isReady").toBool();
            connect(m_iface.data(), SIGNAL(ready()), this, SLOT(onServiceReady()));
            connect(m_iface.data(), SIGNAL(contactsAdded(QStringList)), this, SLOT(onContactsAdded(QStringList)));
            connect(m_iface.data(), SIGNAL(contactsRemoved(QStringList)), this, SLOT(onContactsRemoved(QStringList)));
            connect(m_iface.data(), SIGNAL(contactsUpdated(QStringList)), this, SLOT(onContactsUpdated(QStringList)));

            Q_EMIT serviceChanged();
        } else {
            qWarning() << "Fail to connect with service:"  << m_iface->lastError();
            m_iface.clear();
        }
    }
}

void GaleraContactsService::deinitialize()
{
    Q_FOREACH(QContactRequestData* rData, m_runningRequests) {
        rData->cancel();
        rData->request()->waitForFinished();
        rData->finish(QContactManager::UnspecifiedError);
    }

    if (!m_iface.isNull()) {
        m_id.clear();
        Q_EMIT serviceChanged();
    }

    // this will make the service re-initialize
    m_iface->call("ping");
    if (m_iface->lastError().isValid()) {
        qWarning() << m_iface->lastError();
        m_iface.clear();
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
        QContactFetchByIdRequestData::notifyError(request);
        return;
    }

    QContactIdFilter filter;
    filter.setIds(request->contactIds());
    QString filterStr = Filter(filter).toString();
    QDBusMessage result = m_iface->call("query", filterStr, "", QStringList());
    if (result.type() == QDBusMessage::ErrorMessage) {
        qWarning() << result.errorName() << result.errorMessage();
        QContactFetchByIdRequestData::notifyError(request);
        return;
    }

    QDBusObjectPath viewObjectPath = result.arguments()[0].value<QDBusObjectPath>();
    QDBusInterface *view = new QDBusInterface(m_serviceName,
                                             viewObjectPath.path(),
                                             CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);

    QContactFetchByIdRequestData *data = new QContactFetchByIdRequestData(request, view);
    m_runningRequests << data;
    fetchContactsPage(data);
}

void GaleraContactsService::fetchContacts(QtContacts::QContactFetchRequest *request)
{
    if (!isOnline()) {
        qWarning() << "Server is not online";
        QContactFetchRequestData::notifyError(request);
        return;
    }

    // Only return the sources names if the filter is set as contact group type
    if (request->filter().type() == QContactFilter::ContactDetailFilter) {
        QContactDetailFilter dFilter = static_cast<QContactDetailFilter>(request->filter());

        if ((dFilter.detailType() == QContactDetail::TypeType) &&
            (dFilter.detailField() == QContactType::FieldType) &&
            (dFilter.value() == QContactType::TypeGroup)) {

            QDBusPendingCall pcall = m_iface->asyncCall("availableSources");
            if (pcall.isError()) {
                qWarning() << pcall.error().name() << pcall.error().message();
                QContactFetchRequestData::notifyError(request);
                return;
            }

            QContactFetchRequestData *data = new QContactFetchRequestData(request, 0);
            m_runningRequests << data;

            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
            QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                             [=](QDBusPendingCallWatcher *call) {
                                this->fetchContactsGroupsContinue(data, call);
                             });
            return;
        }
    }

    QString sortStr = SortClause(request->sorting()).toString();
    QString filterStr = Filter(request->filter()).toString();
    FetchHint fetchHint = FetchHint(request->fetchHint()).toString();

    QDBusPendingCall pcall = m_iface->asyncCall("query", filterStr, sortStr, QStringList());
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        QContactFetchRequestData::notifyError(request);
        return;
    }

    QContactFetchRequestData *data = new QContactFetchRequestData(request, 0, fetchHint);
    m_runningRequests << data;

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
    data->updateWatcher(watcher);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsContinue(data, call);
                     });
}


void GaleraContactsService::fetchContactsContinue(QContactFetchRequestData *data,
                                                  QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<QDBusObjectPath> reply = *call;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        destroyRequest(data);
    } else {
        QDBusObjectPath viewObjectPath = reply.value();
        QDBusInterface *view = new QDBusInterface(m_serviceName,
                                                  viewObjectPath.path(),
                                                  CPIM_ADDRESSBOOK_VIEW_IFACE_NAME);
        data->updateView(view);
        fetchContactsPage(data);
    }
}

void GaleraContactsService::fetchContactsPage(QContactFetchRequestData *data)
{
    if (!isOnline() || !data->isLive()) {
        qWarning() << "Server is not online";
        destroyRequest(data);
        return;
    }

    // Load contacs async
    QDBusPendingCall pcall = data->view()->asyncCall("contactsDetails",
                                                     data->fields(),
                                                     data->offset(),
                                                     FETCH_PAGE_SIZE);
    if (pcall.isError()) {
        qWarning() << pcall.error().name() << pcall.error().message();
        data->finish(QContactManager::UnspecifiedError);
        destroyRequest(data);
        return;
    }

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
    data->updateWatcher(watcher);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     [=](QDBusPendingCallWatcher *call) {
                        this->fetchContactsDone(data, call);
                     });
}

void GaleraContactsService::fetchContactsDone(QContactFetchRequestData *data,
                                              QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<QStringList> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        data->update(QList<QContact>(),
                        QContactAbstractRequest::FinishedState,
                        QContactManager::UnspecifiedError);
        destroyRequest(data);
    } else {
        const QStringList vcards = reply.value();
        if (vcards.size()) {
            VCardParser *parser = new VCardParser(this);
            parser->setProperty("DATA", QVariant::fromValue<void*>(data));
            connect(parser, &VCardParser::contactsParsed,
                    this, &GaleraContactsService::onVCardsParsed);
            parser->vcardToContact(vcards);
        } else {
            data->update(QList<QContact>(), QContactAbstractRequest::FinishedState);
            destroyRequest(data);
        }
    }
}

void GaleraContactsService::onVCardsParsed(QList<QContact> contacts)
{
    QObject *sender = QObject::sender();
    QContactFetchRequestData *data = static_cast<QContactFetchRequestData*>(sender->property("DATA").value<void*>());
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

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
        data->update(contacts, QContactAbstractRequest::ActiveState);
        data->updateOffset(FETCH_PAGE_SIZE);
        data->updateWatcher(0);
        fetchContactsPage(data);
    } else {
        data->update(contacts, QContactAbstractRequest::FinishedState);
        destroyRequest(data);
    }

    sender->deleteLater();
}

void GaleraContactsService::fetchContactsGroupsContinue(QContactFetchRequestData *data,
                                                        QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

    QList<QContact> contacts;
    QContactManager::Error opError = QContactManager::NoError;

    QDBusPendingReply<SourceList> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        Q_FOREACH(const Source &source, reply.value()) {
            QContact c = parseSource(source, m_managerUri);
            if (source.isPrimary()) {
                contacts.prepend(c);
            } else {
                contacts << c;
            }
        }
    }

    data->update(contacts, QContactAbstractRequest::FinishedState, opError);
    destroyRequest(data);
}

void GaleraContactsService::saveContact(QtContacts::QContactSaveRequest *request)
{
    QContactSaveRequestData *data = new QContactSaveRequestData(request);
    m_runningRequests << data;

    // first create the new contacts
    data->prepareToCreate();
    createContactsStart(data);
}

void GaleraContactsService::createContactsStart(QContactSaveRequestData *data)
{
    if (!data->isLive()) {
        data->finish(QContactManager::UnspecifiedError);
        destroyRequest(data);
        return;
    }

    if(!data->hasNext()) {
        // go to update contacts
        data->prepareToUpdate();
        updateContacts(data);
        return;
    }

    QString syncSource;
    bool isGroup = false;
    QString contact = data->nextContact(&syncSource, &isGroup);

    if (isGroup) {
       QDBusPendingCall pcall = m_iface->asyncCall("createSource", contact);
       QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
       data->updateWatcher(watcher);
       QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                        [=](QDBusPendingCallWatcher *call) {
                           this->createGroupDone(data, call);
                        });

    } else {
        QDBusPendingCall pcall = m_iface->asyncCall("createContact", contact, syncSource);
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        data->updateWatcher(watcher);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->createContactsDone(data, call);
                         });
    }
}

void GaleraContactsService::createContactsDone(QContactSaveRequestData *data,
                                               QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        data->finish(QContactManager::UnspecifiedError);
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        data->notifyUpdateError(QContactManager::UnspecifiedError);
    } else {
        const QString vcard = reply.value();
        if (!vcard.isEmpty()) {
            QContact contact = VCardParser::vcardToContact(vcard);
            QContactGuid detailId = contact.detail<QContactGuid>();
            GaleraEngineId *engineId = new GaleraEngineId(detailId.guid(), m_managerUri);
            QContactId newId = QContactId(engineId);
            contact.setId(newId);
            data->updateCurrentContact(contact);
        } else {
            data->notifyUpdateError(QContactManager::UnspecifiedError);
        }
    }

    // go to next contact
    createContactsStart(data);
}

void GaleraContactsService::createGroupDone(QContactSaveRequestData *data,
                                            QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        data->finish(QContactManager::UnspecifiedError);
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<Source> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        data->notifyUpdateError(QContactManager::UnspecifiedError);
    } else {
        data->updateCurrentContact(parseSource(reply.value(), m_managerUri));
    }

    // go to next contact
    createContactsStart(data);
}

void GaleraContactsService::removeContact(QContactRemoveRequest *request)
{
    if (!isOnline()) {
        qWarning() << "Server is not online";
        QContactRemoveRequestData::notifyError(request);
        return;
    }

    QContactRemoveRequestData *data = new QContactRemoveRequestData(request);
    m_runningRequests << data;

    QDBusPendingCall pcall = m_iface->asyncCall("removeContacts", data->pendingIds());
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        data->finish(QContactManager::UnspecifiedError);
        destroyRequest(data);
    } else {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        data->updateWatcher(watcher);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->removeContactDone(data, call);
                         });
    }
}

void GaleraContactsService::removeContactDone(QContactRemoveRequestData *data, QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<int> reply = *call;
    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        data->finish(QContactManager::UnspecifiedError);
    } else {
        data->finish();
    }
    destroyRequest(data);
}

void GaleraContactsService::updateContacts(QContactSaveRequestData *data)
{
    if (!isOnline()) {
        destroyRequest(data);
        return;
    }

    QDBusPendingCall pcall = m_iface->asyncCall("updateContacts", data->allPendingContacts());
    if (pcall.isError()) {
        qWarning() <<  "Error" << pcall.error().name() << pcall.error().message();
        data->finish(QtContacts::QContactManager::UnspecifiedError);
        destroyRequest(data);
    } else {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, 0);
        data->updateWatcher(watcher);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                         [=](QDBusPendingCallWatcher *call) {
                            this->updateContactDone(data, call);
                         });
    }
}


void GaleraContactsService::updateContactDone(QContactSaveRequestData *data, QDBusPendingCallWatcher *call)
{
    if (!data->isLive()) {
        destroyRequest(data);
        return;
    }

    QDBusPendingReply<QStringList> reply = *call;
    QContactManager::Error opError = QContactManager::NoError;

    if (reply.isError()) {
        qWarning() << reply.error().name() << reply.error().message();
        opError = QContactManager::UnspecifiedError;
    } else {
        const QStringList vcards = reply.value();
        data->updatePendingContacts(vcards);
    }

    data->finish(opError);
    destroyRequest(data);
}

void GaleraContactsService::cancelRequest(QtContacts::QContactAbstractRequest *request)
{
    Q_FOREACH(QContactRequestData *rData, m_runningRequests) {
        if (rData->request() == request) {
            rData->cancel();
            return;
        }
    }
}

void GaleraContactsService::waitRequest(QtContacts::QContactAbstractRequest *request)
{
    Q_FOREACH(QContactRequestData *rData, m_runningRequests) {
        if (rData->request() == request) {
            rData->wait();
            return;
        }
    }
}

void GaleraContactsService::addRequest(QtContacts::QContactAbstractRequest *request)
{
    if (!isOnline()) {
        qWarning() << "Server is not online";
        QContactManagerEngine::updateRequestState(request, QContactAbstractRequest::FinishedState);
        return;
    }

    if (!m_serviceIsReady) {
        m_pendingRequests << QPointer<QtContacts::QContactAbstractRequest>(request);
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

void GaleraContactsService::destroyRequest(QContactRequestData *request)
{
    m_runningRequests.remove(request);
    delete request;
}

QList<QContactId> GaleraContactsService::parseIds(const QStringList &ids) const
{
    QList<QContactId> contactIds;
    Q_FOREACH(QString id, ids) {
        GaleraEngineId *engineId = new GaleraEngineId(id, m_managerUri);
        contactIds << QContactId(engineId);
    }
    return contactIds;
}

void GaleraContactsService::onContactsAdded(const QStringList &ids)
{
    Q_EMIT contactsAdded(parseIds(ids));
}

void GaleraContactsService::onContactsRemoved(const QStringList &ids)
{
    Q_EMIT contactsRemoved(parseIds(ids));
}

void GaleraContactsService::onContactsUpdated(const QStringList &ids)
{
    Q_EMIT contactsUpdated(parseIds(ids));
}

} //namespace
