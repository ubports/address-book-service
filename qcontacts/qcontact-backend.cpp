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

    qDebug() << Q_FUNC_INFO;
    GaleraManagerEngine *engine = GaleraManagerEngine::createEngine(parameters);
    return engine;
}

QtContacts::QContactEngineId* GaleraEngineFactory::createContactEngineId(const QMap<QString, QString> &parameters,
                                                                         const QString &engineIdString) const
{
    qDebug() << Q_FUNC_INFO;
    return new GaleraEngineId(parameters, engineIdString);
}

QString GaleraEngineFactory::managerName() const
{
    qDebug() << Q_FUNC_INFO;
    return QString::fromLatin1("galera");
}

GaleraManagerEngine* GaleraManagerEngine::createEngine(const QMap<QString, QString> &parameters)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO << this;
    connect(m_service, SIGNAL(contactsAdded(QList<QContactId>)), this, SIGNAL(contactsAdded(QList<QContactId>)));
    connect(m_service, SIGNAL(contactsRemoved(QList<QContactId>)), this, SIGNAL(contactsRemoved(QList<QContactId>)));
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
    qDebug() << Q_FUNC_INFO;
    QMap<QString, QString> parameters;
    return parameters;
}

int GaleraManagerEngine::managerVersion() const
{
    qDebug() << Q_FUNC_INFO;
    return 1;
}

/* Filtering */
QList<QContactId> GaleraManagerEngine::contactIds(const QtContacts::QContactFilter &filter, const QList<QtContacts::QContactSortOrder> &sortOrders, QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;

    /* Special case the fast case */
    if (filter.type() == QtContacts::QContactFilter::DefaultFilter && sortOrders.count() == 0) {
        return m_service->contactIds();
    } else {
        QList<QtContacts::QContact> clist = contacts(filter, sortOrders, QtContacts::QContactFetchHint(), error);

        /* Extract the ids */
        QList<QtContacts::QContactId> ids;
        Q_FOREACH(const QContact &c, clist)
            ids.append(c.id());

        return ids;
    }
}

QList<QtContacts::QContact> GaleraManagerEngine::contacts(const QtContacts::QContactFilter &filter, const QList<QtContacts::QContactSortOrder>& sortOrders, const QContactFetchHint &fetchHint, QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(fetchHint); // no optimizations are possible in the memory backend; ignore the fetch hint.
    Q_UNUSED(error);

    QList<QContact> sorted;

    /* First filter out contacts - check for default filter first */
    if (filter.type() == QContactFilter::DefaultFilter) {
        Q_FOREACH(const QtContacts::QContact&c, m_service->contacts()) {
            QContactManagerEngine::addSorted(&sorted,c, sortOrders);
        }
    } else {
        Q_FOREACH(const QContact&c, m_service->contacts()) {
            if (QContactManagerEngine::testFilter(filter, c)) {
                QContactManagerEngine::addSorted(&sorted,c, sortOrders);
            }
        }
    }

    return sorted;
}

QContact GaleraManagerEngine::contact(const QtContacts::QContactId &contactId, const QContactFetchHint &fetchHint, QtContacts::QContactManager::Error *error) const
{
    qDebug() << Q_FUNC_INFO;

    Q_UNUSED(fetchHint); // no optimizations are possible in the memory backend; ignore the fetch hint.
    int index = m_service->contactIds().indexOf(contactId);
    if (index != -1) {
        // found the contact successfully.
        *error = QContactManager::NoError;
        return m_service->contacts().at(index);
    }

    *error = QContactManager::DoesNotExistError;
    return QContact();
}

bool GaleraManagerEngine::saveContact(QtContacts::QContact *contact, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QContactManager::NoError;
    return true;
}

bool GaleraManagerEngine::removeContact(const QtContacts::QContactId &contactId, QtContacts::QContactManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO;
}

bool GaleraManagerEngine::startRequest(QtContacts::QContactAbstractRequest *req)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO;
    return true;
}

bool GaleraManagerEngine::waitForRequestFinished(QtContacts::QContactAbstractRequest *req, int msecs)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO;
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
