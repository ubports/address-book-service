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

#ifndef __GALERA_QCONTACTSAVEREQUESTDATA_DATA_H__
#define __GALERA_QCONTACTSAVEREQUESTDATA_DATA_H__


#include "qcontactrequest-data.h"
#include <common/fetch-hint.h>
#include <common/source.h>

#include <QtCore/QList>

#include <QtContacts/QContactSaveRequest>

namespace galera
{

class QContactSaveRequestData : public QContactRequestData
{
public:
    QContactSaveRequestData(QtContacts::QContactSaveRequest *request);

    void prepareToUpdate();
    void prepareToCreate();


    bool hasNext() const;
    QString nextContact(QString *syncTargetName);
    QtContacts::QContact currentContact() const;
    QStringList allPendingContacts() const;
    void updateCurrentContact(const QtContacts::QContact &contact);
    void updatePendingContacts(QStringList vcards);

    bool hasNextGroup() const;
    Source nextGroup();
    Source currentGroup() const;
    SourceList allPendingGroups() const;
    void updateCurrentGroup(const Source &group, const QString &managerUri);
    void updatePendingGroups(const SourceList &groups, const QString &managerUri);

    void notifyUpdateError(QtContacts::QContactManager::Error error);
    void notifyError(QtContacts::QContactManager::Error error);
    static void notifyError(QtContacts::QContactSaveRequest *request,
                            QtContacts::QContactManager::Error error = QtContacts::QContactManager::NotSupportedError);

protected:
    void updateRequest(QtContacts::QContactAbstractRequest::State state,
                       QtContacts::QContactManager::Error error,
                       QMap<int, QtContacts::QContactManager::Error> errorMap) override;

private:
    QMap<int, QtContacts::QContact> m_contactsToUpdate;
    QMap<int, QtContacts::QContact> m_contactsToCreate;

    QMap<int, QString> m_pendingContacts;
    QMap<int, QString> m_pendingContactsSyncTarget;
    QMap<int, QString>::Iterator m_currentContact;

    QMap<int, Source> m_pendingGroups;
    QMap<int, Source>::Iterator m_currentGroup;

    void prepareContacts(QMap<int, QtContacts::QContact> contacts);
};

}

Q_DECLARE_METATYPE(galera::QContactSaveRequestData*)

#endif
