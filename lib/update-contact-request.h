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
#include <QtCore/QEventLoop>

#include <QtContacts/QContact>

#include <folks/folks.h>

namespace galera {

class QIndividual;

class UpdateContactRequest : public QObject
{
    Q_OBJECT

public:
    UpdateContactRequest(QtContacts::QContact newContact, QIndividual *parent, QObject *listener, const char *slot);
    ~UpdateContactRequest();
    void start();
    void wait();
    void deatach();
    void notifyError(const QString &errorMessage);

Q_SIGNALS:
    void done(const QString &errorMessage);

private:
    QIndividual *m_parent;
    QObject *m_object;
    FolksPersona *m_currentPersona;
    QEventLoop *m_eventLoop;

    QList<FolksPersona*> m_personas;
    QtContacts::QContact m_originalContact;
    QtContacts::QContact m_newContact;
    int m_currentDetailType;
    QMetaMethod m_slot;
    int m_currentPersonaIndex;

    void invokeSlot(const QString &errorMessage = QString());

    static bool isEqual(QList<QtContacts::QContactDetail> listA,
                        const QtContacts::QContactDetail &prefA,
                        QList<QtContacts::QContactDetail> listB,
                        const QtContacts::QContactDetail &prefB);
    static bool isEqual(QList<QtContacts::QContactDetail> listA,
                        QList<QtContacts::QContactDetail> listB);
    static bool isEqual(const QtContacts::QContactDetail &detailA,
                        const QtContacts::QContactDetail &detailB);
    static bool checkPersona(QtContacts::QContactDetail &det, int persona);
    static QList<QtContacts::QContactDetail> detailsFromPersona(const QtContacts::QContact &contact,
                                                                QtContacts::QContactDetail::DetailType type,
                                                                int persona,
                                                                bool includeEmptyPersona,
                                                                QtContacts::QContactDetail *pref);
    QList<QtContacts::QContactDetail> originalDetailsFromPersona(QtContacts::QContactDetail::DetailType type,
                                                                 int persona,
                                                                 QtContacts::QContactDetail *pref) const;
    QList<QtContacts::QContactDetail> detailsFromPersona(QtContacts::QContactDetail::DetailType type,
                                                         int persona,
                                                         QtContacts::QContactDetail *pref) const;


    void updatePersona();
    void updateAddress();
    void updateAvatar();
    void updateBirthday();
    void updateFullName(const QString &fullName);
    void updateEmail();
    void updateName();
    void updateNickname();
    void updateNote();
    void updateOnlineAccount();
    void updateOrganization();
    void updatePhone();
    void updateUrl();
    void updateFavorite();

    QString callDetailChangeFinish(QtContacts::QContactDetail::DetailType detailType,
                                   FolksPersona *persona,
                                   GAsyncResult *result);

    static void updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata);
};

}

#endif
