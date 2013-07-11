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

    FolksPersona *getPersona(int index) const;
    int personaCount() const;

    static GHashTable *parseDetails(const QtContacts::QContact &contact);
private:
    FolksIndividual *m_individual;
    FolksPersona *m_primaryPersona;
    FolksIndividualAggregator *m_aggregator;
    QtContacts::QContact m_contact;
    QMap<QString, QPair<QtContacts::QContactDetail, FolksAbstractFieldDetails*> > m_fieldsMap;

    QMultiHash<QString, QString> parseDetails(FolksAbstractFieldDetails *details) const;
    void updateContact();

    FolksPersona *primaryPersona();
    QtContacts::QContactDetail detailFromUri(QtContacts::QContactDetail::DetailType type, const QString &uri) const;

    void appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QtContacts::QContactDetail detail, bool readOnly) const;
    void appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QList<QtContacts::QContactDetail> details, bool readOnly) const;

    // QContact
    QList<QtContacts::QContactDetail> getDetails() const;
    QtContacts::QContactDetail getUid() const;
    QList<QtContacts::QContactDetail> getClientPidMap() const;
    QtContacts::QContactDetail getPersonaName(FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaFullName(FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaNickName(FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaBirthday(FolksPersona *persona, int index) const;
    QtContacts::QContactDetail getPersonaPhoto(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaRoles(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaEmails(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaPhones(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaAddresses(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaIms(FolksPersona *persona, int index) const;
    QList<QtContacts::QContactDetail> getPersonaUrls(FolksPersona *persona, int index) const;

    static void avatarCacheStoreDone(GObject *source, GAsyncResult *result, gpointer data);

    // create
    void createPersonaFromDetails(QList<QtContacts::QContactDetail> detail, ParseDetailsFunc parseFunc, void *data) const;
    static void createPersonaForDetailDone(GObject *detail, GAsyncResult *result, gpointer userdata);

    // translate details
    static GHashTable *parseAddressDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parsePhotoDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parsePhoneNumbersDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseOrganizationDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseImDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNoteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseFullNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNicknameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseGenderDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseFavoriteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseEmailDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseBirthdayDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
    static GHashTable *parseUrlDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails);
};

} //namespace
#endif

