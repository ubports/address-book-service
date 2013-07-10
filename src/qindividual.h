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
typedef GeeSet* (*GetDetailsSetFunction) (void*);
typedef void (*DetailsChangeFunction) (void*, GeeSet*, GAsyncReadyCallback, gpointer);

typedef QList<QtVersit::QVersitProperty> PropertyList;
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
    FolksIndividual *individual() const;
    void setIndividual(FolksIndividual *individual);

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

    // update
    void updateFullName(const QtContacts::QContactDetail &name, void* data);
    void updateName(const QtContacts::QContactDetail &name, void* data);
    void updateNickname(const QtContacts::QContactDetail &detail, void* data);
    void updateBirthday(const QtContacts::QContactDetail &detail, void* data);
    void updatePhoto(const QtContacts::QContactDetail &detail, void* data);
    void updateTimezone(const QtContacts::QContactDetail &detail, void* data);
    void updateRole(QtContacts::QContactDetail detail, void* data);
    void updatePhone(QtContacts::QContactDetail detail, void* data);
    void updateEmail(QtContacts::QContactDetail detail, void* data);
    void updateIm(QtContacts::QContactDetail detail, void* data);
    void updateUrl(QtContacts::QContactDetail details, void* data);
    void updateNote(QtContacts::QContactDetail detail, void* data);
    void updateAddress(QtContacts::QContactDetail detail, void* data);

    // create
    void createPersonaForDetail(QList<QtContacts::QContactDetail> detail, ParseDetailsFunc parseFunc, void *data) const;
    static void createPersonaForDetailDone(GObject *detail, GAsyncResult *result, gpointer userdata);

    // remove
    void removeDetail(QtContacts::QContactDetail detail, GType detailGType, GetDetailsSetFunction getDetailsSetFunction, DetailsChangeFunction changeFunction, void* data);
    void removeRole(QtContacts::QContactDetail detail, void* data);
    void removePhone(QtContacts::QContactDetail detail, void* data);
    void removeEmail(QtContacts::QContactDetail detail, void* data);
    void removeIm(QtContacts::QContactDetail detail, void* data);
    void removeUrl(QtContacts::QContactDetail detail, void* data);
    void removeNote(QtContacts::QContactDetail detail, void* data);
    void removeAddress(QtContacts::QContactDetail detail, void* data);

    static void updateDone(GObject *detail, GAsyncResult *result, gpointer userdata);
    static void updateDetailsSendReply(gpointer userdata, GError *error);
    static void updateDetailsSendReply(gpointer userdata, const QString &errorMessage);
    static QString callDetailChangeFinish(QtContacts::QContactDetail::DetailType type, FolksPersona *persona, GAsyncResult *result);

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

    static QStringList listParameters(FolksAbstractFieldDetails *details);
    static void parseParameters(QtContacts::QContactDetail &detail, FolksAbstractFieldDetails *fd);
    static void parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &parameters);
    static void parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters);
    static void parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters);

    static QList<int> contextsFromParameters(QStringList &parameters);
    static QStringList listContext(const QtContacts::QContactDetail &detail);
    static void parseContext(FolksAbstractFieldDetails *fd, const QtContacts::QContactDetail &detail);
    static QStringList parseContext(const QtContacts::QContactDetail &detail);
    static QStringList parsePhoneContext(const QtContacts::QContactDetail &detail);
    static QStringList parseAddressContext(const QtContacts::QContactDetail &detail);
    static QStringList parseOnlineAccountContext(const QtContacts::QContactDetail &detail);

    static int onlineAccountProtocolFromString(const QString &protocol);
    static QString onlineAccountProtocolFromEnum(int protocol);

    friend class QIndividualUtils;
};

} //namespace
#endif

