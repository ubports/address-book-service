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

#include "qcontact-backend.h"

#include <QContact>
#include <QContactManagerEngine>
#include <QContactAbstractRequest>
#include <QContactChangeSet>
#include <QContactTimestamp>
#include <QContactIdFilter>

#include <QtCore/qdebug.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/quuid.h>
#include <QtCore/QCoreApplication>

#include <QVersitDocument>
#include <QVersitProperty>

#include "contacts-service.h"
#include "qcontact-engineid.h"

using namespace QtContacts;

namespace galera
{

QtContacts::QContactManagerEngine* GaleraEngineFactory::engine(const QMap<QString, QString> &parameters, QtContacts::QContactManager::Error *error)
{
    Q_UNUSED(error);

    GaleraManagerEngine *engine = GaleraManagerEngine::createEngine(parameters);
    return engine;
}

QtContacts::QContactEngineId* GaleraEngineFactory::createContactEngineId(const QMap<QString, QString> &parameters,
                                                                         const QString &engineIdString) const
{
    return new GaleraEngineId(parameters, engineIdString);
}

QString GaleraEngineFactory::managerName() const
{
    return QString::fromLatin1("galera");
}

GaleraManagerEngine* GaleraManagerEngine::createEngine(const QMap<QString, QString> &parameters)
{
    GaleraManagerEngine *engine = new GaleraManagerEngine();
    return engine;
}

/*!
 * Constructs a new in-memory backend which shares the given \a data with
 * other shared memory engines.
 */
GaleraManagerEngine::GaleraManagerEngine()
    : m_service(new GaleraContactsService(managerUri()))
{
    connect(m_service, SIGNAL(contactsAdded(QList<QContactId>)), this, SIGNAL(contactsAdded(QList<QContactId>)));
    connect(m_service, SIGNAL(contactsRemoved(QList<QContactId>)), this, SIGNAL(contactsRemoved(QList<QContactId>)));
    connect(m_service, SIGNAL(contactsUpdated(QList<QContactId>)), this, SIGNAL(contactsChanged(QList<QContactId>)));
    connect(m_service, SIGNAL(serviceChanged()), this, SIGNAL(dataChanged()));
}

/*! Frees any memory used by this engine */
GaleraManagerEngine::~GaleraManagerEngine()
{
    delete m_service;
}

/* URI reporting */
QString GaleraManagerEngine::managerName() const
{
    return "galera";
}

QMap<QString, QString> GaleraManagerEngine::managerParameters() const
{
    QMap<QString, QString> parameters;
    return parameters;
}

int GaleraManagerEngine::managerVersion() const
{
    return 1;
}

/* Filtering */
QList<QContactId> GaleraManagerEngine::contactIds(const QtContacts::QContactFilter &filter, const QList<QtContacts::QContactSortOrder> &sortOrders, QtContacts::QContactManager::Error *error) const
{
    QContactFetchHint hint;
    hint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactDetail::TypeGuid);
    QList<QtContacts::QContact> clist = contacts(filter, sortOrders, hint, error);

    /* Extract the ids */
    QList<QtContacts::QContactId> ids;
    Q_FOREACH(const QContact &c, clist)
        ids.append(c.id());

    return ids;
}

QList<QtContacts::QContact> GaleraManagerEngine::contacts(const QtContacts::QContactFilter &filter,
                                                          const QList<QtContacts::QContactSortOrder>& sortOrders,
                                                          const QContactFetchHint &fetchHint, QtContacts::QContactManager::Error *error) const
{
    Q_UNUSED(fetchHint);
    Q_UNUSED(error);

    QContactFetchRequest request;
    request.setFilter(filter);
    request.setSorting(sortOrders);

    const_cast<GaleraManagerEngine*>(this)->startRequest(&request);
    const_cast<GaleraManagerEngine*>(this)->waitForRequestFinished(&request, -1);

    if (error) {
        *error = request.error();
    }

    return request.contacts();
}

QList<QContact> GaleraManagerEngine::contacts(const QList<QContactId> &contactIds, const QContactFetchHint &fetchHint, QMap<int, QContactManager::Error> *errorMap, QContactManager::Error *error) const
{
    QContactFetchByIdRequest request;
    request.setIds(contactIds);
    request.setFetchHint(fetchHint);

    const_cast<GaleraManagerEngine*>(this)->startRequest(&request);
    const_cast<GaleraManagerEngine*>(this)->waitForRequestFinished(&request, -1);

    if (errorMap) {
        *errorMap = request.errorMap();
    }

    if (error) {
        *error = request.error();
    }

    return request.contacts();
}

QContact GaleraManagerEngine::contact(const QContactId &contactId, const QContactFetchHint &fetchHint, QContactManager::Error *error) const
{
    QContactFetchByIdRequest request;
    request.setIds(QList<QContactId>() << contactId);
    request.setFetchHint(fetchHint);

    const_cast<GaleraManagerEngine*>(this)->startRequest(&request);
    const_cast<GaleraManagerEngine*>(this)->waitForRequestFinished(&request, -1);

    if (error) {
        *error = request.error();
    }

    return request.contacts().value(0, QContact());
}

bool GaleraManagerEngine::saveContact(QtContacts::QContact *contact, QtContacts::QContactManager::Error *error)
{
    QContactSaveRequest request;

    request.setContact(*contact);
    startRequest(&request);
    waitForRequestFinished(&request, -1);
    *error = QContactManager::NoError;

    // FIXME: GaleraContactsService::updateContactDone doesn't return contacts
    if (contact->id().isNull())
      *contact = request.contacts()[0];

    return true;
}

bool GaleraManagerEngine::removeContact(const QtContacts::QContactId &contactId, QtContacts::QContactManager::Error *error)
{
    *error = QContactManager::NoError;
    contact(contactId, QContactFetchHint(), error);
    if (*error == QContactManager::DoesNotExistError) {
        return false;
    }

    QContactRemoveRequest request;

    request.setContactId(contactId);
    startRequest(&request);
    waitForRequestFinished(&request, -1);
    *error = QContactManager::NoError;

    return true;
}

bool GaleraManagerEngine::saveRelationship(QtContacts::QContactRelationship *relationship, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

bool GaleraManagerEngine::removeRelationship(const QtContacts::QContactRelationship &relationship, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}


bool GaleraManagerEngine::saveContacts(QList<QtContacts::QContact> *contacts, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

bool GaleraManagerEngine::saveContacts(QList<QtContacts::QContact> *contacts,  const QList<QtContacts::QContactDetail::DetailType> &typeMask, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

bool GaleraManagerEngine::removeContacts(const QList<QtContacts::QContactId> &contactIds, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

/* "Self" contact id (MyCard) */
bool GaleraManagerEngine::setSelfContactId(const QtContacts::QContactId &contactId, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

QtContacts::QContactId GaleraManagerEngine::selfContactId(QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return QContactId();
}

/* Relationships between contacts */
QList<QtContacts::QContactRelationship> GaleraManagerEngine::relationships(const QString &relationshipType, const QContact& participant, QContactRelationship::Role role, QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return QList<QContactRelationship>();
}

bool GaleraManagerEngine::saveRelationships(QList<QtContacts::QContactRelationship> *relationships, QMap<int, QtContacts::QContactManager::Error>* errorMap, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

bool GaleraManagerEngine::removeRelationships(const QList<QtContacts::QContactRelationship> &relationships, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

/* Validation for saving */
bool GaleraManagerEngine::validateContact(const QtContacts::QContact &contact, QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;

    *error = QContactManager::NoError;
    return true;
}

/* Asynchronous Request Support */
void GaleraManagerEngine::requestDestroyed(QtContacts::QContactAbstractRequest *req)
{
}

bool GaleraManagerEngine::startRequest(QtContacts::QContactAbstractRequest *req)
{
    if (!req) {
        return false;
    }

    QPointer<QContactAbstractRequest> checkDeletion(req);
    updateRequestState(req, QContactAbstractRequest::ActiveState);
    if (!checkDeletion.isNull()) {
        m_service->addRequest(req);
    }
    return true;
}

bool GaleraManagerEngine::cancelRequest(QtContacts::QContactAbstractRequest *req)
{
    if (req) {
        m_service->cancelRequest(req);
        return true;
    } else {
        return false;
    }
}

bool GaleraManagerEngine::waitForRequestFinished(QtContacts::QContactAbstractRequest *req, int msecs)
{
    Q_UNUSED(msecs);
    m_service->waitRequest(req);
    return true;
}

/* Capabilities reporting */
bool GaleraManagerEngine::isRelationshipTypeSupported(const QString &relationshipType, QtContacts::QContactType::TypeValues contactType) const
{
    qDebug() << Q_FUNC_INFO;
    return true;
}

bool GaleraManagerEngine::isFilterSupported(const QtContacts::QContactFilter &filter) const
{
    qDebug() << Q_FUNC_INFO;
    return true;
}

QList<QVariant::Type> GaleraManagerEngine::supportedDataTypes() const
{
    QList<QVariant::Type> st;
    st.append(QVariant::String);
    st.append(QVariant::Date);
    st.append(QVariant::DateTime);
    st.append(QVariant::Time);
    st.append(QVariant::Bool);
    st.append(QVariant::Char);
    st.append(QVariant::Int);
    st.append(QVariant::UInt);
    st.append(QVariant::LongLong);
    st.append(QVariant::ULongLong);
    st.append(QVariant::Double);

    return st;
}

} // namespace
