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

#include "update-contact-request.h"
#include "qindividual.h"
#include "detail-context-parser.h"
#include "gee-utils.h"

#include "common/vcard-parser.h"

#include <QtContacts/qcontactdetails.h>

using namespace QtContacts;

namespace galera {

template<>
bool sortDetails<QContactDetail::TypePhoneNumber>(const QContactDetail &d1, const QContactDetail &d2)
{
    return d1.value(QtContacts::QContactPhoneNumber::FieldNumber).toString() <
           d2.value(QtContacts::QContactPhoneNumber::FieldNumber).toString();
}

template<>
bool sortDetails<QContactDetail::TypeEmailAddress>(const QContactDetail &d1, const QContactDetail &d2)
{
    return d1.value(QtContacts::QContactEmailAddress::FieldEmailAddress).toString() <
           d2.value(QtContacts::QContactEmailAddress::FieldEmailAddress).toString();
}


UpdateContactRequest::UpdateContactRequest(QtContacts::QContact newContact, QIndividual *parent, QObject *listener, const char *slot)
    : QObject(),
      m_parent(parent),
      m_object(listener),
      m_currentPersona(0),
      m_eventLoop(0),
      m_newContact(newContact),
      m_currentPersonaIndex(0)
{
    int slotIndex = listener->metaObject()->indexOfSlot(++slot);
    if (slotIndex == -1) {
        qWarning() << "Invalid slot:" << slot << "for object" << listener;
    } else {
        m_slot = listener->metaObject()->method(slotIndex);
    }
}

UpdateContactRequest::~UpdateContactRequest()
{
    // check if there is a operation running
    if (m_currentPersona != 0) {
        wait();
    }
}

void UpdateContactRequest::invokeSlot(const QString &errorMessage)
{
    Q_EMIT done(errorMessage);

    if (m_slot.isValid() && m_parent) {
        m_slot.invoke(m_object, Q_ARG(QString, m_parent->id()),
                                Q_ARG(QString, errorMessage));
    } else if (m_parent == 0) {
        // the object was detached we need to destroy it
        deleteLater();
    }

    if (m_eventLoop) {
        m_eventLoop->quit();
    }
}

void UpdateContactRequest::start()
{
    m_currentDetailType = QContactDetail::TypeAddress;
    m_originalContact = m_parent->contact();
    m_personas = m_parent->personas();
    m_currentPersonaIndex = 0;
    updatePersona();
}

void UpdateContactRequest::wait()
{
    Q_ASSERT(m_eventLoop == 0);
    QEventLoop eventLoop;
    m_eventLoop = &eventLoop;
    eventLoop.exec();
    m_eventLoop = 0;
}

void UpdateContactRequest::deatach()
{
    m_parent = 0;
}

void UpdateContactRequest::notifyError(const QString &errorMessage)
{
    invokeSlot(errorMessage);
}

bool UpdateContactRequest::isEqual(const QtContacts::QContactDetail &detailA,
                                   const QtContacts::QContactDetail &detailB)
{
    if (detailA.type() != detailB.type()) {
        return false;
    }
    switch(detailA.type()) {
    case QContactDetail::TypeFavorite:
        return detailA.value(QContactFavorite::FieldFavorite).toBool() == detailB.value(QContactFavorite::FieldFavorite).toBool();
    default:
    {
        // clear detail uri to compare only the field value
        QContactDetail copyA(detailA);
        QContactDetail copyB(detailB);

        copyA.setDetailUri("");
        copyB.setDetailUri("");
        return (copyA == copyB);
    }
    }
}

bool UpdateContactRequest::isEqual(QList<QtContacts::QContactDetail> listA,
                                   QList<QtContacts::QContactDetail> listB)
{
    if (listA.size() != listB.size()) {
        return false;
    }

    if (listA.size() == 0) {
        return true;
    }

    const int detailType = listA.first().type();
    switch(detailType) {
    case QContactDetail::TypePhoneNumber:
        qSort(listA.begin(), listA.end(), sortDetails<QContactDetail::TypePhoneNumber>);
        qSort(listB.begin(), listB.end(), sortDetails<QContactDetail::TypePhoneNumber>);
        break;
    case QContactDetail::TypeEmailAddress:
        qSort(listA.begin(), listA.end(), sortDetails<QContactDetail::TypeEmailAddress>);
        qSort(listB.begin(), listB.end(), sortDetails<QContactDetail::TypeEmailAddress>);
        break;
    default:
        qSort(listA.begin(), listA.end(), sortDetails<0>);
        qSort(listB.begin(), listB.end(), sortDetails<0>);
        break;
    }

    for(int i=0; i < listA.size(); i++) {
        if (!isEqual(listA[i], listB[i])) {
            return false;
        }
    }

    return true;
}

bool UpdateContactRequest::isEqual(QList<QtContacts::QContactDetail> listA,
                                   const QtContacts::QContactDetail &prefA,
                                   QList<QtContacts::QContactDetail> listB,
                                   const QtContacts::QContactDetail &prefB)
{
    if (prefA != prefB) {
        return false;
    }

    return isEqual(listA, listB);
}

bool UpdateContactRequest::checkPersona(QtContacts::QContactDetail &det, int persona)
{
    if (det.detailUri().isEmpty()) {
        return false;
    }

    QStringList ids = det.detailUri().split(".");
    Q_ASSERT(ids.count() == 2);

    return (ids[0].toInt() == persona);
}

QList<QtContacts::QContactDetail> UpdateContactRequest::detailsFromPersona(const QtContacts::QContact &contact,
                                                                           QtContacts::QContactDetail::DetailType type,
                                                                           int persona,
                                                                           bool includeEmptyPersona,
                                                                           QtContacts::QContactDetail *pref)
{
    QList<QtContacts::QContactDetail> personaDetails;
    QContactDetail globalPref;
    if (pref && VCardParser::PreferredActionNames.contains(type)) {
        globalPref = contact.preferredDetail(VCardParser::PreferredActionNames[type]);
    }

    Q_FOREACH(QContactDetail det, contact.details(type)) {
        if ((includeEmptyPersona && det.detailUri().isEmpty()) ||
            checkPersona(det, persona)) {
            if (!det.isEmpty()) {
                personaDetails << det;
                if (pref && det == globalPref) {
                    *pref = det;
                }
            }
        }
    }
    return personaDetails;
}

QList<QtContacts::QContactDetail> UpdateContactRequest::originalDetailsFromPersona(QtContacts::QContactDetail::DetailType type,
                                                                                   int persona,
                                                                                   QtContacts::QContactDetail *pref) const
{
    return detailsFromPersona(m_originalContact, type, persona, false, pref);
}

QList<QtContacts::QContactDetail> UpdateContactRequest::detailsFromPersona(QtContacts::QContactDetail::DetailType type,
                                                                           int persona,
                                                                           QtContacts::QContactDetail *pref) const
{
    // only return new details for the first persona, this will avoid to create the details for all personas
    return detailsFromPersona(m_newContact, type, persona, (persona==1), pref);
}

void UpdateContactRequest::updateAddress()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeAddress,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);
    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeAddress,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_POSTAL_ADDRESS_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "Adderess diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactAddress addr = static_cast<QContactAddress>(newDetail);

            FolksPostalAddress *pa;
            QByteArray postOfficeBox = addr.postOfficeBox().toUtf8();
            QByteArray street = addr.street().toUtf8();
            QByteArray locality = addr.locality().toUtf8();
            QByteArray region = addr.region().toUtf8();
            QByteArray postcode = addr.postcode().toUtf8();
            QByteArray country = addr.country().toUtf8();

            pa = folks_postal_address_new(postOfficeBox.constData(),
                                          NULL,
                                          street.constData(),
                                          locality.constData(),
                                          region.constData(),
                                          postcode.constData(),
                                          country.constData(),
                                          NULL,
                                          NULL);

            FolksPostalAddressFieldDetails *field = folks_postal_address_field_details_new(pa, NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, newDetail == prefDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
            g_object_unref(pa);
        }

        folks_postal_address_details_change_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(m_currentPersona),
                                                             newSet,
                                                             (GAsyncReadyCallback) updateDetailsDone,
                                                             this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateAvatar()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeAvatar, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeAvatar, m_currentPersonaIndex, 0);

    if (m_currentPersona &&
        FOLKS_IS_AVATAR_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, newDetails)) {
        qDebug() << "avatar diff:"
                 << "\n\t" << originalDetails.size() << (originalDetails.size() > 0 ? originalDetails[0] : QContactDetail()) << "\n"
                 << "\n\t" << newDetails.size() << (newDetails.size() > 0 ? newDetails[0] : QContactDetail());

        // update avatar version if necessary
        QContactExtendedDetail originalAvatarRev;
        QContactExtendedDetail newAvatarRev;
        Q_FOREACH(const QContactExtendedDetail &det,
                  originalDetailsFromPersona(QContactDetail::TypeExtendedDetail, m_currentPersonaIndex, 0)) {
            if (det.name() == "X-AVATAR-REV") {
                originalAvatarRev = det;
                break;
            }
        }
        Q_FOREACH(const QContactExtendedDetail &det,
                  detailsFromPersona(QContactDetail::TypeExtendedDetail, m_currentPersonaIndex, 0)) {
            if (det.name() == "X-AVATAR-REV") {
                newAvatarRev = det;
                break;
            }
        }
        // if the avatar changed and the rev still the same we need to reset it to force a sync
        if (originalAvatarRev.data() == newAvatarRev.data()) {
            newAvatarRev.setData("");
            m_newContact.saveDetail(&newAvatarRev);
        }

        //Only supports one avatar
        QUrl avatarUri;
        QUrl oldAvatarUri;

        if (originalDetails.count()) {
            QContactAvatar avatar = static_cast<QContactAvatar>(originalDetails[0]);
            oldAvatarUri = avatar.imageUrl();
        }

        if (newDetails.count()) {
            QContactAvatar avatar = static_cast<QContactAvatar>(newDetails[0]);
            avatarUri = avatar.imageUrl();
        }

        if ((avatarUri != oldAvatarUri) &&
            (avatarUri.isLocalFile() || avatarUri.isEmpty())){
            GFileIcon *avatarFileIcon = NULL;
            if(!avatarUri.isEmpty()) {
                QString formattedUri = avatarUri.toString(QUrl::RemoveUserInfo);

                if(!formattedUri.isEmpty()) {
                    QByteArray uriUtf8 = formattedUri.toUtf8();
                    GFile *avatarFile = g_file_new_for_uri(uriUtf8.constData());
                    avatarFileIcon = G_FILE_ICON(g_file_icon_new(avatarFile));
                    g_object_unref(avatarFile);
                }
            }

            folks_avatar_details_change_avatar(FOLKS_AVATAR_DETAILS(m_currentPersona),
                                               G_LOADABLE_ICON(avatarFileIcon),
                                               (GAsyncReadyCallback) updateDetailsDone,
                                               this);
            if (avatarFileIcon) {
                g_object_unref(avatarFileIcon);
            }
            return;
        }
    }
    updateDetailsDone(0, 0, this);
}

void UpdateContactRequest::updateBirthday()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeBirthday, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeBirthday, m_currentPersonaIndex, 0);

    if (m_currentPersona &&
        FOLKS_IS_BIRTHDAY_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, newDetails)) {
        qDebug() << "birthday diff";
        //Only supports one birthday
        QDateTime dateTimeBirthday;
        if (newDetails.count()) {
            QContactBirthday birthday = static_cast<QContactBirthday>(newDetails[0]);
            if (!birthday.isEmpty()) {
                dateTimeBirthday = birthday.dateTime();
            }
        }

        GDateTime *dateTime = NULL;
        if (dateTimeBirthday.isValid()) {
            dateTime = g_date_time_new_from_unix_utc(dateTimeBirthday.toMSecsSinceEpoch() / 1000);
        }
        folks_birthday_details_change_birthday(FOLKS_BIRTHDAY_DETAILS(m_currentPersona),
                                               dateTime,
                                               (GAsyncReadyCallback) updateDetailsDone,
                                               this);
        if (dateTime) {
            g_date_time_unref(dateTime);
        }
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateFullName(const QString &fullName)
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeDisplayLabel, m_currentPersonaIndex, 0);
    if (m_currentPersona &&
        FOLKS_IS_NAME_DETAILS(m_currentPersona) &&
        originalDetails.size() > 0 &&
        (originalDetails[0].value(QContactDisplayLabel::FieldLabel).toString() != fullName)) {
        qDebug() << "Full Name diff:"
                 << "\n\t" << originalDetails.size() << (originalDetails.size() > 0 ? originalDetails[0] : QContactDetail()) << "\n"
                 << "\n\t" << fullName;
        //Only supports one fullName
        QByteArray fullNameUtf8 = fullName.toUtf8();
        folks_name_details_change_full_name(FOLKS_NAME_DETAILS(m_currentPersona),
                                            fullNameUtf8.constData(),
                                            (GAsyncReadyCallback) updateDetailsDone,
                                            this);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateEmail()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeEmailAddress,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);

    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeEmailAddress,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_EMAIL_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "email diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactEmailAddress email = static_cast<QContactEmailAddress>(newDetail);
            FolksEmailFieldDetails *field;

            QByteArray emailAddress = email.emailAddress().toUtf8();
            field = folks_email_field_details_new(emailAddress.constData(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, newDetail == prefDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_email_details_change_email_addresses(FOLKS_EMAIL_DETAILS(m_currentPersona),
                                                   newSet,
                                                   (GAsyncReadyCallback) updateDetailsDone,
                                                   this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateName()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeName, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeName, m_currentPersonaIndex, 0);

    if (m_currentPersona &&
        FOLKS_IS_NAME_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, newDetails)) {
        //Only supports one fullName
        FolksStructuredName *sn = 0;
        if (newDetails.count()) {
            QContactName name = static_cast<QContactName>(newDetails[0]);

            QByteArray lastName = name.lastName().toUtf8();
            QByteArray firstName = name.firstName().toUtf8();
            QByteArray middleName = name.middleName().toUtf8();
            QByteArray prefix = name.prefix().toUtf8();
            QByteArray suffix = name.suffix().toUtf8();
            sn = folks_structured_name_new(lastName.constData(),
                                           firstName.constData(),
                                           middleName.constData(),
                                           prefix.constData(),
                                           suffix.constData());
        }

        folks_name_details_change_structured_name(FOLKS_NAME_DETAILS(m_currentPersona),
                                                  sn,
                                                  (GAsyncReadyCallback) updateDetailsDone,
                                                  this);
        if (sn) {
            g_object_unref(sn);
        }
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateNickname()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeNickname, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeNickname, m_currentPersonaIndex, 0);

    if (m_currentPersona &&
        FOLKS_IS_NAME_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, newDetails)) {
        qDebug() << "Nickname diff";
        //Only supports one fullName
        QString nicknameValue;
        if (newDetails.count()) {
            QContactNickname nickname = static_cast<QContactNickname>(newDetails[0]);
            nicknameValue = nickname.nickname();
        }

        QByteArray nicknameValueUtf8 = nicknameValue.toUtf8();
        folks_name_details_change_nickname(FOLKS_NAME_DETAILS(m_currentPersona),
                                           nicknameValueUtf8.constData(),
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           this);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateNote()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeNote, m_currentPersonaIndex, &originalPref);
    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeNote, m_currentPersonaIndex, &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_EMAIL_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "notes diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactNote note = static_cast<QContactNote>(newDetail);
            FolksNoteFieldDetails *field;

            QByteArray noteUtf8 = note.note().toUtf8();
            field = folks_note_field_details_new(noteUtf8.constData(), 0, 0);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, newDetail == prefDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_note_details_change_notes(FOLKS_NOTE_DETAILS(m_currentPersona),
                                        newSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateOnlineAccount()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeOnlineAccount,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);
    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeOnlineAccount,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_IM_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "OnlineAccounts diff";
        GeeMultiMap *imMap = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactOnlineAccount account = static_cast<QContactOnlineAccount>(newDetail);
            FolksImFieldDetails *field;

            if (account.protocol() != QContactOnlineAccount::ProtocolUnknown) {
                QByteArray accountUri = account.accountUri().toUtf8();
                field = folks_im_field_details_new(accountUri.constData(), NULL);
                DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), account, account == prefDetail);

                QString protocolName(DetailContextParser::accountProtocolName(account.protocol()));
                QByteArray protocolNameUtf8 = protocolName.toUtf8();
                gee_multi_map_set(imMap, protocolNameUtf8.constData(), field);

                g_object_unref(field);
            }
        }

       folks_im_details_change_im_addresses(FOLKS_IM_DETAILS(m_currentPersona),
                                            imMap,
                                            (GAsyncReadyCallback) updateDetailsDone,
                                            this);

        g_object_unref(imMap);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateOrganization()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeOrganization,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);
    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeOrganization,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_ROLE_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "Organization diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactOrganization org = static_cast<QContactOrganization>(newDetail);
            FolksRoleFieldDetails *field;
            FolksRole *roleValue;

            QByteArray title = org.title().isEmpty() ? "" : org.title().toUtf8();
            QByteArray name = org.name().isEmpty() ? "" : org.name().toUtf8();
            QByteArray roleName = org.role().isEmpty() ? "" : org.role().toUtf8();

            roleValue = folks_role_new(title.constData(), name.constData(), "");
            folks_role_set_role(roleValue, roleName.constData());
            field = folks_role_field_details_new(roleValue, NULL);

            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, newDetail == prefDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
            g_object_unref(roleValue);
        }

        folks_role_details_change_roles(FOLKS_ROLE_DETAILS(m_currentPersona),
                                        newSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        this);

        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updatePhone()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypePhoneNumber,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);
    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypePhoneNumber,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_PHONE_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "Phone diff:"
                 << "\n\t" << originalDetails.size() << (originalDetails.size() > 0 ? originalDetails[0] : QContactDetail()) << "\n"
                 << "\n\t" << newDetails.size() << (newDetails.size() > 0 ? newDetails[0] : QContactDetail());

        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(newDetail);
            FolksPhoneFieldDetails *field;

            QByteArray phoneNumber = phone.number().toUtf8();
            field = folks_phone_field_details_new(phoneNumber.constData(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, newDetail == prefDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_phone_details_change_phone_numbers(FOLKS_PHONE_DETAILS(m_currentPersona),
                                                 newSet,
                                                 (GAsyncReadyCallback) updateDetailsDone,
                                                 this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateUrl()
{
    QContactDetail originalPref;
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeUrl,
                                                                       m_currentPersonaIndex,
                                                                       &originalPref);

    QContactDetail prefDetail;
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeUrl,
                                                          m_currentPersonaIndex,
                                                          &prefDetail);

    if (m_currentPersona &&
        FOLKS_IS_URL_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, originalPref, newDetails, prefDetail)) {
        qDebug() << "Url diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactUrl url = static_cast<QContactUrl>(newDetail);
            FolksUrlFieldDetails *field;

            QByteArray urlValue = url.url().toUtf8();
            field = folks_url_field_details_new(urlValue.constData(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail, prefDetail == newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_url_details_change_urls(FOLKS_URL_DETAILS(m_currentPersona),
                                      newSet,
                                      (GAsyncReadyCallback) updateDetailsDone,
                                      this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateFavorite()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeFavorite, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeFavorite, m_currentPersonaIndex, 0);

    // as default all contacts has favorte set to false
    if (newDetails.isEmpty()) {
        QContactFavorite fav;
        fav.setFavorite(false);
        newDetails << fav;
    }

    if (m_currentPersona &&
        FOLKS_IS_FAVOURITE_DETAILS(m_currentPersona) &&
        !isEqual(originalDetails, newDetails)) {
        qDebug() << "Favorite diff:"
                 << "\n\t" << originalDetails.size() << (originalDetails.size() > 0 ? originalDetails[0] : QContactDetail()) << "\n"
                 << "\n\t" << newDetails.size() << (newDetails.size() > 0 ? newDetails[0] : QContactDetail());

        //Only supports one fullName
        bool isFavorite = false;
        if (newDetails.count()) {
            QContactFavorite favorite = static_cast<QContactFavorite>(newDetails[0]);
            isFavorite = favorite.isFavorite();
        }
        folks_favourite_details_change_is_favourite(FOLKS_FAVOURITE_DETAILS(m_currentPersona),
                                                    isFavorite,
                                                    (GAsyncReadyCallback) updateDetailsDone,
                                                    this);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateExtendedDetails()
{
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeExtendedDetail, m_currentPersonaIndex, 0);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeExtendedDetail, m_currentPersonaIndex, 0);
    if (m_currentPersona &&
        !isEqual(originalDetails, newDetails)) {
        qDebug() << "Extended details diff";
        QIndividual::setExtendedDetails(m_currentPersona, newDetails);
    }
    updateDetailsDone(0, 0, this);
}

void UpdateContactRequest::updatePersona()
{
    if (m_personas.size() <= m_currentPersonaIndex) {
        m_currentPersona = 0;
        if (m_parent) {
            m_parent->flush();
        }
        invokeSlot();
    } else {
        m_currentPersona = m_personas[m_currentPersonaIndex];
        g_object_ref(m_currentPersona);
        m_currentDetailType = QContactDetail::TypeUndefined;
        m_currentPersonaIndex++;
        updateDetailsDone(0, 0, this);
    }
}

QString UpdateContactRequest::callDetailChangeFinish(QtContacts::QContactDetail::DetailType detailType,
                                                     FolksPersona *persona,
                                                     GAsyncResult *result)
{
    Q_ASSERT(persona);
    Q_ASSERT(result);

    GError *error = 0;
    switch(detailType) {
    case QContactDetail::TypeAddress:
        folks_postal_address_details_change_postal_addresses_finish(FOLKS_POSTAL_ADDRESS_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeAvatar:
        folks_avatar_details_change_avatar_finish(FOLKS_AVATAR_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeBirthday:
        folks_birthday_details_change_birthday_finish(FOLKS_BIRTHDAY_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeDisplayLabel:
        folks_name_details_change_full_name_finish(FOLKS_NAME_DETAILS(persona), result, &error);
       break;
    case QContactDetail::TypeEmailAddress:
        folks_email_details_change_email_addresses_finish(FOLKS_EMAIL_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeFavorite:
        folks_favourite_details_change_is_favourite_finish(FOLKS_FAVOURITE_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeName:
        folks_name_details_change_structured_name_finish(FOLKS_NAME_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeNickname:
        folks_name_details_change_nickname_finish(FOLKS_NAME_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeNote:
        folks_note_details_change_notes_finish(FOLKS_NOTE_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeOnlineAccount:
        folks_im_details_change_im_addresses_finish(FOLKS_IM_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeOrganization:
        folks_role_details_change_roles_finish(FOLKS_ROLE_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypePhoneNumber:
        folks_phone_details_change_phone_numbers_finish(FOLKS_PHONE_DETAILS(persona), result, &error);
        break;
    case QContactDetail::TypeUrl:
        folks_url_details_change_urls_finish(FOLKS_URL_DETAILS(persona), result, &error);
    default:
        break;
    }

    QString errorMessage;
    if (error) {
        errorMessage = QString::fromUtf8(error->message);
        g_error_free(error);
    }
    return errorMessage;
}

void UpdateContactRequest::updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata)
{
    UpdateContactRequest *self = static_cast<UpdateContactRequest*>(userdata);

    QString errorMessage;
    if (detail && result) {
        if (FOLKS_IS_PERSONA(detail)) {
            // This is a normal field update
            errorMessage = self->callDetailChangeFinish(static_cast<QContactDetail::DetailType>(self->m_currentDetailType),
                                                        FOLKS_PERSONA(detail),
                                                        result);
        }

        if (!errorMessage.isEmpty()) {
            qWarning() << "Fail to update contact" << errorMessage;
            self->invokeSlot(errorMessage);
            return;
        }
    }

    self->m_currentDetailType += 1;
    switch(static_cast<QContactDetail::DetailType>(self->m_currentDetailType)) {
    case QContactDetail::TypeAddress:
        self->updateAddress();
        break;
    case QContactDetail::TypeAvatar:
        self->updateAvatar();
        break;
    case QContactDetail::TypeBirthday:
        self->updateBirthday();
        break;
    case QContactDetail::TypeDisplayLabel:
        self->updateFullName(QIndividual::displayName(self->m_newContact));
        break;
    case QContactDetail::TypeEmailAddress:
        //WORKAROUND: Folks automatically add online accounts based on e-mail address
        // for example user@gmail.com will create a jabber account, and this causes some
        // confusions on the service during the update, because of that we first update
        // the online account and this will avoid problems with the automatic update ]
        // from folks
        self->updateOnlineAccount();
        break;
    case QContactDetail::TypeExtendedDetail:
        self->updateExtendedDetails();
        break;
    case QContactDetail::TypeFavorite:
        self->updateFavorite();
        break;
    case QContactDetail::TypeName:
        self->updateName();
        break;
    case QContactDetail::TypeNickname:
        self->updateNickname();
        break;
    case QContactDetail::TypeNote:
        self->updateNote();
        break;
    case QContactDetail::TypeOnlineAccount:
        //WORKAROUND: see TypeEmailAddress update clause
        self->updateEmail();
        break;
    case QContactDetail::TypeOrganization:
        self->updateOrganization();
        break;
    case QContactDetail::TypePhoneNumber:
        self->updatePhone();
        break;
    case QContactDetail::TypeUrl:
        self->updateUrl();
        break;
    case QContactDetail::TypeAnniversary:
    case QContactDetail::TypeFamily:
    case QContactDetail::TypeGender:
    case QContactDetail::TypeGeoLocation:
    case QContactDetail::TypeGlobalPresence:
    case QContactDetail::TypeHobby:
    case QContactDetail::TypeRingtone:
    case QContactDetail::TypeTag:
    case QContactDetail::TypeTimestamp:
        //TODO
    case QContactDetail::TypeGuid:
    case QContactDetail::TypeType:
        updateDetailsDone(0, 0, self);
        break;
    case QContactDetail::TypeVersion:
        g_object_unref(self->m_currentPersona);
        self->m_currentPersona = 0;
        self->updatePersona();
        break;
    default:
        qWarning() << "Update not implemented for" << self->m_currentDetailType;
        updateDetailsDone(0, 0, self);
        break;
    }
}

} // namespace
