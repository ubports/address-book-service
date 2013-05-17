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


#ifndef __GALER_QCONTACT_BACKEND_H__
#define __GALER_QCONTACT_BACKEND_H__

#include <QtContacts/QContact>
#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactManagerEngineFactoryInterface>
#include <QtContacts/qcontactengineid.h>
#include <QtContacts/QContactAbstractRequest>

#include <QtDBus/QDBusInterface>

namespace galera
{
class GaleraContactsService;

class GaleraEngineFactory : public QtContacts::QContactManagerEngineFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QContactManagerEngineFactoryInterface" FILE "galera.json")
public:
    QtContacts::QContactManagerEngine* engine(const QMap<QString, QString> &parameters, QtContacts::QContactManager::Error*);
    QString managerName() const;
    QtContacts::QContactEngineId* createContactEngineId(const QMap<QString, QString> &parameters, const QString &engineIdString) const;
};

class GaleraManagerEngine : public QtContacts::QContactManagerEngine
{
    Q_OBJECT

public:
    static GaleraManagerEngine *createEngine(const QMap<QString, QString> &parameters);

    ~GaleraManagerEngine();

    /* URI reporting */
    QString managerName() const;   
    QMap<QString, QString> managerParameters() const;

    /*! \reimp */
    int managerVersion() const;

    /* Filtering */
    virtual QList<QtContacts::QContactId> contactIds(const QtContacts::QContactFilter &filter, const QList<QtContacts::QContactSortOrder> &sortOrders, QtContacts::QContactManager::Error *error) const;
    virtual QList<QtContacts::QContact> contacts(const QtContacts::QContactFilter &filter, const QList<QtContacts::QContactSortOrder>& sortOrders, const QtContacts::QContactFetchHint &fetchHint, QtContacts::QContactManager::Error *error) const;
    virtual QtContacts::QContact contact(const QtContacts::QContactId &contactId, const QtContacts::QContactFetchHint &fetchHint, QtContacts::QContactManager::Error *error) const;

    virtual bool saveContact(QtContacts::QContact *contact, QtContacts::QContactManager::Error *error);
    virtual bool removeContact(const QtContacts::QContactId &contactId, QtContacts::QContactManager::Error *error);
    virtual bool saveRelationship(QtContacts::QContactRelationship *relationship, QtContacts::QContactManager::Error *error);
    virtual bool removeRelationship(const QtContacts::QContactRelationship &relationship, QtContacts::QContactManager::Error *error);

    virtual bool saveContacts(QList<QtContacts::QContact> *contacts, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error);
    virtual bool saveContacts(QList<QtContacts::QContact> *contacts,  const QList<QtContacts::QContactDetail::DetailType> &typeMask, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error);
    virtual bool removeContacts(const QList<QtContacts::QContactId> &contactIds, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error);

    /* "Self" contact id (MyCard) */
    virtual bool setSelfContactId(const QtContacts::QContactId &contactId, QtContacts::QContactManager::Error *error);
    virtual QtContacts::QContactId selfContactId(QtContacts::QContactManager::Error *error) const;

    /* Relationships between contacts */
    virtual QList<QtContacts::QContactRelationship> relationships(const QString &relationshipType, const QtContacts::QContact& participant, QtContacts::QContactRelationship::Role role, QtContacts::QContactManager::Error *error) const;
    virtual bool saveRelationships(QList<QtContacts::QContactRelationship> *relationships, QMap<int, QtContacts::QContactManager::Error>* errorMap, QtContacts::QContactManager::Error *error);
    virtual bool removeRelationships(const QList<QtContacts::QContactRelationship> &relationships, QMap<int, QtContacts::QContactManager::Error> *errorMap, QtContacts::QContactManager::Error *error);

    /* Validation for saving */
    virtual bool validateContact(const QtContacts::QContact &contact, QtContacts::QContactManager::Error *error) const;

    /* Asynchronous Request Support */
    virtual void requestDestroyed(QtContacts::QContactAbstractRequest *req);
    virtual bool startRequest(QtContacts::QContactAbstractRequest *req);
    virtual bool cancelRequest(QtContacts::QContactAbstractRequest *req);
    virtual bool waitForRequestFinished(QtContacts::QContactAbstractRequest *req, int msecs);

    /* Capabilities reporting */
    virtual bool isRelationshipTypeSupported(const QString &relationshipType, QtContacts::QContactType::TypeValues contactType) const;
    virtual bool isFilterSupported(const QtContacts::QContactFilter &filter) const;
    virtual QList<QVariant::Type> supportedDataTypes() const;

private:
    GaleraManagerEngine();

    GaleraContactsService *m_service;
};

} //namespace

#endif

