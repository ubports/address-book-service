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

#ifndef __GALERA_QINDIVIDUAL_H__
#define __GALERA_QINDIVIDUAL_H__

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMultiHash>
#include <QVersitProperty>

#include <QtContacts/QContact>
#include <QtContacts/QContactDetail>

#include <folks/folks.h>

namespace galera
{
typedef GHashTable* (*ParseDetailsFunc)(GHashTable*, const QList<QtContacts::QContactDetail> &);

class UpdateContactRequest;

class QIndividual
{
public:
    QIndividual(FolksIndividual *individual, FolksIndividualAggregator *aggregator);
    ~QIndividual();

    QString id() const;
    QtContacts::QContact &contact();
    QtContacts::QContact copy(QList<QtContacts::QContactDetail::DetailType> fields);
    bool update(const QString &vcard, QObject *object, const char *slot);
    bool update(const QtContacts::QContact &contact, QObject *object, const char *slot);
    void setIndividual(FolksIndividual *individual);
    FolksIndividual *individual() const;
    QList<FolksPersona*> personas() const;
    void addListener(QObject *object, const char *slot);
    bool isValid() const;
    void flush();

    static GHashTable *parseDetails(const QtContacts::QContact &contact);

    // enable or disable auto-link
    static void enableAutoLink(bool flag);
    static bool autoLinkEnabled();

private:
    FolksIndividual *m_individual;
    FolksIndividualAggregator *m_aggregator;
    QtContacts::QContact *m_contact;
    UpdateContactRequest *m_currentUpdate;
    QList<QPair<QObject*, QMetaMethod> > m_listeners;
    QMap<QString, FolksPersona*> m_personas;
    QMap<FolksPersona*, QList<int> > m_notifyConnections;
    QString m_id;
    QMetaObject::Connection m_updateConnection;
    static bool m_autoLink;

    QIndividual();
    QIndividual(const QIndividual &);

    void notifyUpdate();

    QMultiHash<QString, QString> parseDetails(FolksAbstractFieldDetails *details) const;
    void updateContact();
    void updatePersonas();
    void clearPersonas();
    void clear();

    FolksPersona *primaryPersona();
    QtContacts::QContactDetail detailFromUri(QtContacts::QContactDetail::DetailType type, const QString &uri) const;

    void appendDetailsForPersona(QtContacts::QContact *contact,
                                 const QtContacts::QContactDetail &detail,
                                 bool readOnly) const;
    void appendDetailsForPersona(QtContacts::QContact *contact,
                                 QList<QtContacts::QContactDetail> details,
                                 const QString &preferredAction,
                                 const QtContacts::QContactDetail &preferred,
                                 bool readOnly) const;

    // QContact
    QtContacts::QContactDetail getUid() const;
    QList<QtContacts::QContactDetail> getClientPidMap() const;
    QtContacts::QContactDetail getPersonaName           (FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaFullName       (FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaNickName       (FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaBirthday       (FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaPhoto          (FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaFavorite       (FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaRoles   (FolksPersona *persona,
                                                         QtContacts::QContactDetail *preferredRole,
                                                         int index) const;
    QList<QtContacts::QContactDetail> getPersonaEmails  (FolksPersona *persona,
                                                         QtContacts::QContactDetail *preferredEmail,
                                                         int index) const;
    QList<QtContacts::QContactDetail> getPersonaPhones  (FolksPersona *persona,
                                                         QtContacts::QContactDetail *preferredPhone,
                                                         int index) const;
    QList<QtContacts::QContactDetail> getPersonaAddresses(FolksPersona *persona,
                                                          QtContacts::QContactDetail *preferredAddress,
                                                          int index) const;
    QList<QtContacts::QContactDetail> getPersonaIms     (FolksPersona *persona,
                                                         QtContacts::QContactDetail *preferredIm,
                                                         int index) const;
    QList<QtContacts::QContactDetail> getPersonaUrls    (FolksPersona *persona,
                                                         QtContacts::QContactDetail *preferredUrl,
                                                         int index) const;

    static void avatarCacheStoreDone(GObject *source, GAsyncResult *result, gpointer data);

    // create
    void createPersonaFromDetails(QList<QtContacts::QContactDetail> detail, ParseDetailsFunc parseFunc, void *data) const;
    static void createPersonaForDetailDone(GObject *detail, GAsyncResult *result, gpointer userdata);

    // translate details
    static GHashTable *parseFullNameDetails     (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNicknameDetails     (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNameDetails         (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseGenderDetails       (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseFavoriteDetails     (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parsePhotoDetails        (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseBirthdayDetails     (GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseAddressDetails      (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parsePhoneNumbersDetails (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parseOrganizationDetails (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parseImDetails           (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parseNoteDetails         (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parseEmailDetails        (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    static GHashTable *parseUrlDetails          (GHashTable *details,
                                                 const QList<QtContacts::QContactDetail> &cDetails,
                                                 const QtContacts::QContactDetail &prefDetail);
    // property changed
    static void folksPersonaChanged             (FolksPersona *persona,
                                                 GParamSpec *pspec,
                                                 QIndividual *self);
};

} //namespace
#endif

