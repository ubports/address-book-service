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

#ifndef __GALERA_UPDATE_CONTACT_REQUEST_H__
#define __GALERA_UPDATE_CONTACT_REQUEST_H__

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>

#include <QtContacts/QContact>

#include <folks/folks.h>

namespace galera {

class QIndividual;

class UpdateContactRequest : public QObject
{
    Q_OBJECT

public:
    UpdateContactRequest(QtContacts::QContact newContact, QIndividual *parent, QObject *listener, const char *slot);

    void start();

Q_SIGNALS:
    void done(const QString &errorMessage);

private:
    QtContacts::QContact m_newContact;
    int m_currentDetailType;
    int m_currentPersonaIndex;
    int m_maxPersona;
    QIndividual *m_parent;
    QObject *m_object;
    QMetaMethod m_slot;

    void invokeSlot(const QString &errorMessage = QString());

    static bool isEqual(QList<QtContacts::QContactDetail> listA, QList<QtContacts::QContactDetail> listB);
    static bool checkPersona(QtContacts::QContactDetail &det, int persona);
    static QList<QtContacts::QContactDetail> detailsFromPersona(const QtContacts::QContact &contact, QtContacts::QContactDetail::DetailType type, int persona, bool includeEmptyPersona);
    QList<QtContacts::QContactDetail> originalDetailsFromPersona(QtContacts::QContactDetail::DetailType type, int persona) const;
    QList<QtContacts::QContactDetail> detailsFromPersona(QtContacts::QContactDetail::DetailType type, int persona) const;

    void updatePersona(int index);
    void updateAddress();
    void updateAvatar();
    void updateBirthday();
    void updateFullName();
    void updateEmail();
    void updateName();
    void updateNickname();
    void updateNote();
    void updateOnlineAccount();
    void updateOrganization();
    void updatePhone();
    void updateUrl();

    QString callDetailChangeFinish(QtContacts::QContactDetail::DetailType detailType,
                                   FolksPersona *persona,
                                   GAsyncResult *result);

    static void updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata);

};

}

#endif
