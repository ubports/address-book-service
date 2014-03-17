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
    if (!contacts.isEmpty()) {
        QStringList vcards = VCardParser::contactToVcard(contacts.values());
        if (vcards.count() != contacts.count()) {
            qWarning() << "Fail to parse contacts to Update";
            return;
        }
        int i = 0;
        Q_FOREACH(int index, contacts.keys()) {
            m_pendingContacts.insert(index, vcards[i]);
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
    qDebug() << "Contacts to create" << m_pendingContacts.count();
}


bool QContactSaveRequestData::hasNext() const
{
    return (m_pendingContacts.count() > 0);
}

QString QContactSaveRequestData::nextContact()
{
    Q_ASSERT(m_pendingContacts.count() > 0);
    qDebug() << "SIZEEEE" << m_pendingContacts.count() << "Has next:" << hasNext();
    m_currentContact = m_pendingContacts.begin();

    qDebug() << "next contact" << m_currentContact.key() << m_currentContact.value();
    return m_currentContact.value();
}

void QContactSaveRequestData::updateCurrentContactId(GaleraEngineId *engineId)
{
    QContactId contactId(engineId);
    QContact &contact = m_contactsToCreate[m_currentContact.key()];
    contact.setId(contactId);
    m_pendingContacts.remove(m_currentContact.key());
    qDebug() << "After update id" << m_pendingContacts.count();
}

void QContactSaveRequestData::updatePendingContacts(QStringList vcards)
{
    if (!vcards.isEmpty()) {
        QList<QContact> contacts = VCardParser::vcardToContact(vcards);
        if (contacts.count() != m_pendingContacts.count()) {
            qWarning() << "Fail to parse vcards to contacts";
        }

        // update the contacts on update list
        QList<int> indexs = m_contactsToUpdate.keys();
        Q_FOREACH(const QContact &contact, contacts) {
            m_contactsToUpdate.insert(indexs.takeFirst(), contact);
        }
    }
}

void QContactSaveRequestData::notifyUpdateError(QContactManager::Error error)
{
    m_contactsToUpdate.remove(m_currentContact.key());
    m_errorMap.insert(m_currentContact.key(), error);
    m_pendingContacts.remove(m_currentContact.key());
}

QStringList QContactSaveRequestData::allPendingContacts() const
{
    return m_pendingContacts.values();
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
