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

#include "qcontactsaverequest-data.h"
#include "common/vcard-parser.h"
#include "qcontact-engineid.h"

#include <QtCore/QDebug>

#include <QtContacts/QContactManagerEngine>
#include <QtContacts/QContactSyncTarget>

#include <QtVersit/QVersitContactImporter>

using namespace QtContacts;
using namespace QtVersit;

namespace galera
{

QContactSaveRequestData::QContactSaveRequestData(QContactSaveRequest *request)
    : QContactRequestData(request)
{
    int index = 0;
    Q_FOREACH(const QContact &contact, request->contacts()) {
        if (contact.id().isNull()) {
            m_contactsToCreate.insert(index, contact);
        } else {
            m_contactsToUpdate.insert(index, contact);
        }
        index++;
    }
}

void QContactSaveRequestData::prepareContacts(QMap<int, QtContacts::QContact> contacts)
{
    Q_FOREACH(int index, contacts.keys()) {
        QContact contact = contacts[index];
        if (contact.type() == QContactType::TypeGroup) {
            m_pendingGroups.insert(index, Source::fromQContact(contact));
        } else {
            m_pendingContacts.insert(index, VCardParser::contactToVcard(contact));
            m_pendingContactsSyncTarget.insert(index, contact.detail<QContactSyncTarget>().syncTarget());
        }
    }
}

void QContactSaveRequestData::prepareToUpdate()
{
    Q_ASSERT(m_pendingContacts.count() == 0);
    prepareContacts(m_contactsToUpdate);
}

void QContactSaveRequestData::prepareToCreate()
{
    Q_ASSERT(m_pendingContacts.count() == 0);
    prepareContacts(m_contactsToCreate);
}

bool QContactSaveRequestData::hasNext() const
{
    return (m_pendingContacts.count() > 0);
}

bool QContactSaveRequestData::hasNextGroup() const
{
    return (m_pendingGroups.count() > 0);
}

QString QContactSaveRequestData::nextContact(QString *syncTargetName)
{
    Q_ASSERT(m_pendingContacts.count() > 0);
    m_currentContact = m_pendingContacts.begin();

    if (syncTargetName) {
        *syncTargetName = m_pendingContactsSyncTarget.begin().value();
    }
    return m_currentContact.value();
}

Source QContactSaveRequestData::nextGroup()
{
    Q_ASSERT(m_pendingGroups.count() > 0);
    m_currentGroup = m_pendingGroups.begin();

    return *m_currentGroup;
}

void QContactSaveRequestData::updateCurrentContactId(GaleraEngineId *engineId)
{
    QContactId contactId(engineId);
    QContact &contact = m_contactsToCreate[m_currentContact.key()];
    contact.setId(contactId);
    m_pendingContacts.remove(m_currentContact.key());
    m_pendingContactsSyncTarget.remove(m_currentContact.key());
}

void QContactSaveRequestData::updateCurrentContact(const QContact &contact)
{
    m_contactsToCreate[m_currentContact.key()] = contact;
    m_pendingContacts.remove(m_currentContact.key());
    m_pendingContactsSyncTarget.remove(m_currentContact.key());
}

void QContactSaveRequestData::updateCurrentGroup(const Source &group, const QString &managerUri)
{
    galera::GaleraEngineId *engineId = new galera::GaleraEngineId(QString("source@%1").arg(group.id()),
                                                                  managerUri);
    QContactId id = QContactId(engineId);
    m_contactsToCreate[m_currentGroup.key()] = group.toContact(id);
    m_pendingGroups.remove(m_currentGroup.key());
}

void QContactSaveRequestData::updatePendingGroups(const SourceList &groups, const QString &managerUri)
{
    if (groups.size() != m_pendingGroups.size()) {
        qWarning() << "Fail to update groups";
        return;
    }

    int index = 0;
    Q_FOREACH(int key, m_pendingGroups.keys()) {
        const Source &group = groups.at(index);
        galera::GaleraEngineId *engineId = new galera::GaleraEngineId(QString("source@%1").arg(group.id()),
                                                                      managerUri);
        QContactId id = QContactId(engineId);
        m_contactsToUpdate.insert(key, group.toContact(id));
        index++;
    }
}

void QContactSaveRequestData::updatePendingContacts(QStringList vcards)
{
    if (!vcards.isEmpty()) {
        QList<QContact> contacts = VCardParser::vcardToContactSync(vcards);
        if (contacts.count() != m_pendingContacts.count()) {
            qWarning() << "Fail to parse vcards to contacts";
        }

        // update the contacts on update list
        QList<int> indexes = m_contactsToUpdate.keys();
        Q_FOREACH(const QContact &contact, contacts) {
            m_contactsToUpdate.insert(indexes.takeFirst(), contact);
        }
    }
}

void QContactSaveRequestData::notifyUpdateError(QContactManager::Error error)
{
    m_contactsToUpdate.remove(m_currentContact.key());
    m_errorMap.insert(m_currentContact.key(), error);
    m_pendingContacts.remove(m_currentContact.key());
    m_pendingContactsSyncTarget.remove(m_currentContact.key());
}

QContact QContactSaveRequestData::currentContact() const
{
    return qobject_cast<QContactSaveRequest*>(request())->contacts().at(m_currentContact.key());
}

QStringList QContactSaveRequestData::allPendingContacts() const
{
    return m_pendingContacts.values();
}

SourceList QContactSaveRequestData::allPendingGroups() const
{
    return m_pendingGroups.values();
}

void QContactSaveRequestData::notifyError(QContactSaveRequest *request, QContactManager::Error error)
{
    QContactManagerEngine::updateContactSaveRequest(request,
                                                    QList<QContact>(),
                                                    error,
                                                    QMap<int, QContactManager::Error>(),
                                                    QContactAbstractRequest::FinishedState);
}

void QContactSaveRequestData::updateRequest(QContactAbstractRequest::State state, QContactManager::Error error, QMap<int, QContactManager::Error> errorMap)
{
    // join back contacts to update and create
    QMap<int, QtContacts::QContact> allContacts = m_contactsToUpdate;

    Q_FOREACH(int key, m_contactsToCreate.keys()) {
        allContacts.insert(key, m_contactsToCreate[key]);
    }

    QContactManagerEngine::updateContactSaveRequest(static_cast<QContactSaveRequest*>(m_request.data()),
                                                    allContacts.values(),
                                                    error,
                                                    errorMap,
                                                    state);
}

} //namespace
