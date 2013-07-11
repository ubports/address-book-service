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

#include <QtContacts/qcontactdetails.h>

using namespace QtContacts;

namespace {



}

namespace galera {

UpdateContactRequest::UpdateContactRequest(QtContacts::QContact newContact, QIndividual *parent, QObject *listener, const char *slot)
    : QObject(),
      m_newContact(newContact),
      m_parent(parent),
      m_object(listener)
{
    int slotIndex = listener->metaObject()->indexOfSlot(++slot);
    if (slotIndex == -1) {
        qWarning() << "Invalid slot:" << slot << "for object" << listener;
    } else {
        m_slot = listener->metaObject()->method(slotIndex);
    }
}

void UpdateContactRequest::invokeSlot(const QString &errorMessage)
{
    if (m_slot.isValid()) {
        m_slot.invoke(m_object, Q_ARG(galera::QIndividual*, m_parent), Q_ARG(QString, errorMessage));
    }
    Q_EMIT done(errorMessage);
}

void UpdateContactRequest::start()
{
    m_currentDetailType = QContactDetail::TypeAddress;
    updatePersona(1);
}

bool UpdateContactRequest::isEqual(QList<QtContacts::QContactDetail> listA, QList<QtContacts::QContactDetail> listB)
{
    if (listA.size() != listB.size()) {
        return false;
    }

    for(int i=0; i < listA.size(); i++) {
        if (listA[i] != listB[i]) {
            return false;
        }
    }

    return true;
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
                                                                           bool includeEmptyPersona)
{
    QList<QtContacts::QContactDetail> personaDetails;

    Q_FOREACH(QContactDetail det, contact.details(type)) {
        if ((includeEmptyPersona && det.detailUri().isEmpty()) || checkPersona(det, persona)) {
            if (!det.isEmpty()) {
                personaDetails << det;
            }
        }
    }
    return personaDetails;
}

QList<QtContacts::QContactDetail> UpdateContactRequest::originalDetailsFromPersona(QtContacts::QContactDetail::DetailType type, int persona) const
{
    return detailsFromPersona(m_parent->contact(), type, persona, false);
}

QList<QtContacts::QContactDetail> UpdateContactRequest::detailsFromPersona(QtContacts::QContactDetail::DetailType type, int persona) const
{
    return detailsFromPersona(m_newContact, type, persona, true);
}

void UpdateContactRequest::updateAddress()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeAddress, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeAddress, m_currentPersonaIndex);

    if (persona && FOLKS_IS_POSTAL_ADDRESS_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Adderess diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactAddress addr = static_cast<QContactAddress>(newDetail);

            FolksPostalAddress *field;
            field = folks_postal_address_new(addr.postOfficeBox().toUtf8().data(),
                                             NULL,
                                             addr.street().toUtf8().data(),
                                             addr.locality().toUtf8().data(),
                                             addr.region().toUtf8().data(),
                                             addr.postcode().toUtf8().data(),
                                             addr.country().toUtf8().data(),
                                             NULL,
                                             NULL);

            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_postal_address_details_change_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeAvatar, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeAvatar, m_currentPersonaIndex);

    if (persona && FOLKS_IS_AVATAR_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "avatar diff";
        //Only supports one avatar
        QContactAvatar avatar = static_cast<QContactAddress>(newDetails[0]);
        QUrl avatarUri = avatar.imageUrl();

        GFileIcon *avatarFileIcon = NULL;
        if(!avatarUri.isEmpty()) {
            QString formattedUri = avatarUri.toString(QUrl::RemoveUserInfo);

            if(!formattedUri.isEmpty()) {
                GFile *avatarFile = g_file_new_for_uri(formattedUri.toUtf8().data());
                avatarFileIcon = G_FILE_ICON(g_file_icon_new(avatarFile));
                g_object_unref(avatarFile);
            }
        }

        folks_avatar_details_change_avatar(FOLKS_AVATAR_DETAILS(persona),
                                           G_LOADABLE_ICON(avatarFileIcon),
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           this);
        if (avatarFileIcon) {
            g_object_unref(avatarFileIcon);
        }
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateBirthday()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeBirthday, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeBirthday, m_currentPersonaIndex);

    if (persona && FOLKS_IS_BIRTHDAY_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "birthday diff";
        //Only supports one birthday
        QContactBirthday birthday = static_cast<QContactBirthday>(newDetails[0]);

        GDateTime *dateTime = NULL;
        if (!birthday.isEmpty()) {
            dateTime = g_date_time_new_from_unix_utc(birthday.dateTime().toMSecsSinceEpoch() / 1000);
        }
        folks_birthday_details_change_birthday(FOLKS_BIRTHDAY_DETAILS(persona),
                                               dateTime,
                                               (GAsyncReadyCallback) updateDetailsDone,
                                               this);
        g_date_time_unref(dateTime);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateFullName()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeDisplayLabel, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeDisplayLabel, m_currentPersonaIndex);

    if (persona && FOLKS_IS_NAME_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Full Name diff";
        //Only supports one fullName
        QContactDisplayLabel label = static_cast<QContactDisplayLabel>(newDetails[0]);

        folks_name_details_change_full_name(FOLKS_NAME_DETAILS(persona),
                                            label.label().toUtf8().data(),
                                            (GAsyncReadyCallback) updateDetailsDone,
                                            this);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateEmail()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeEmailAddress, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeEmailAddress, m_currentPersonaIndex);

    if (persona && FOLKS_IS_EMAIL_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "email diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactEmailAddress email = static_cast<QContactEmailAddress>(newDetail);
            FolksEmailFieldDetails *field;

            field = folks_email_field_details_new(email.emailAddress().toUtf8().data(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_email_details_change_email_addresses(FOLKS_EMAIL_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeName, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeName, m_currentPersonaIndex);

    if (persona && FOLKS_IS_NAME_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Name diff";
        //Only supports one fullName
        QContactName name = static_cast<QContactName>(newDetails[0]);

        FolksStructuredName *sn;
        sn = folks_structured_name_new(name.lastName().toUtf8().data(),
                                       name.firstName().toUtf8().data(),
                                       name.middleName().toUtf8().data(),
                                       name.prefix().toUtf8().data(),
                                       name.suffix().toUtf8().data());

        folks_name_details_change_structured_name(FOLKS_NAME_DETAILS(persona),
                                                  sn,
                                                  (GAsyncReadyCallback) updateDetailsDone,
                                                  this);
        g_object_unref(sn);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateNickname()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeNickname, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeNickname, m_currentPersonaIndex);

    if (persona && FOLKS_IS_NAME_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Nickname diff";
        //Only supports one fullName
        QContactNickname nickname = static_cast<QContactNickname>(newDetails[0]);

        folks_name_details_change_nickname(FOLKS_NAME_DETAILS(persona),
                                           nickname.nickname().toUtf8().data(),
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           this);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updateNote()
{
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeNote, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeNote, m_currentPersonaIndex);

    if (persona && FOLKS_IS_EMAIL_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "notes diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactNote note = static_cast<QContactNote>(newDetail);
            FolksNoteFieldDetails *field;

            field = folks_note_field_details_new(note.note().toUtf8().data(), 0, 0);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_note_details_change_notes(FOLKS_NOTE_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeOnlineAccount, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeOnlineAccount, m_currentPersonaIndex);

    if (persona && FOLKS_IS_IM_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "OnlineAccounts diff";
        GeeMultiMap *imMap = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactOnlineAccount account = static_cast<QContactOnlineAccount>(newDetail);
            FolksImFieldDetails *field;

            if (account.protocol() != QContactOnlineAccount::ProtocolUnknown) {
                field = folks_im_field_details_new(account.accountUri().toUtf8().data(), NULL);
                DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), account);

                QString protocolName(DetailContextParser::accountProtocolName(account.protocol()));
                qDebug() << "Append protocol" << protocolName << account.accountUri();
                gee_multi_map_set(imMap, protocolName.toUtf8().data(), field);

                g_object_unref(field);
            }
        }

       folks_im_details_change_im_addresses(FOLKS_IM_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeOrganization, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeOrganization, m_currentPersonaIndex);

    if (persona && FOLKS_IS_ROLE_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Organization diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactOrganization org = static_cast<QContactOrganization>(newDetail);
            FolksRoleFieldDetails *field;
            FolksRole *roleValue;

            const gchar* title = org.title().isEmpty() ? "" : org.title().toUtf8().data();
            const gchar* name = org.name().isEmpty() ? "" : org.name().toUtf8().data();
            const gchar* roleName = org.role().isEmpty() ? "" : org.role().toUtf8().data();

            roleValue = folks_role_new(title, name, "");
            folks_role_set_role(roleValue, roleName);
            field = folks_role_field_details_new(roleValue, NULL);

            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
            g_object_unref(roleValue);
        }

        folks_role_details_change_roles(FOLKS_ROLE_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypePhoneNumber, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypePhoneNumber, m_currentPersonaIndex);

    if (persona && FOLKS_IS_PHONE_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Phone diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(newDetail);
            FolksPhoneFieldDetails *field;

            field = folks_phone_field_details_new(phone.number().toUtf8().data(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_phone_details_change_phone_numbers(FOLKS_PHONE_DETAILS(persona),
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
    FolksPersona *persona = m_parent->getPersona(m_currentPersonaIndex);
    QList<QContactDetail> originalDetails = originalDetailsFromPersona(QContactDetail::TypeUrl, m_currentPersonaIndex);
    QList<QContactDetail> newDetails = detailsFromPersona(QContactDetail::TypeUrl, m_currentPersonaIndex);

    if (persona && FOLKS_IS_PHONE_DETAILS(persona) && !isEqual(originalDetails, newDetails)) {
        qDebug() << "Url diff";
        GeeSet *newSet = SET_AFD_NEW();

        Q_FOREACH(QContactDetail newDetail, newDetails) {
            QContactUrl url = static_cast<QContactUrl>(newDetail);
            FolksUrlFieldDetails *field;

            field = folks_url_field_details_new(url.url().toUtf8().data(), NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(field), newDetail);
            gee_collection_add(GEE_COLLECTION(newSet), field);
            g_object_unref(field);
        }

        folks_url_details_change_urls(FOLKS_URL_DETAILS(persona),
                                      newSet,
                                      (GAsyncReadyCallback) updateDetailsDone,
                                      this);
        g_object_unref(newSet);
    } else {
        updateDetailsDone(0, 0, this);
    }
}

void UpdateContactRequest::updatePersona(int index)
{
    if (index > m_parent->personaCount()) {
        invokeSlot();
    } else {
        m_currentPersonaIndex = index;
        m_currentDetailType = QContactDetail::TypeUndefined;
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
        self->updateFullName();
        break;
    case QContactDetail::TypeEmailAddress:
        self->updateEmail();
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
        self->updateOnlineAccount();
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
    case QContactDetail::TypeFavorite: //IMPORTANT
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
    case QContactDetail::TypeExtendedDetail:
        updateDetailsDone(0, 0, self);
        break;
    case QContactDetail::TypeVersion:
        self->m_currentPersonaIndex += 1;
        self->updatePersona(self->m_currentPersonaIndex);
        break;
    default:
        qWarning() << "Update not implemented for" << self->m_currentDetailType;
        updateDetailsDone(0, 0, self);
        break;
    }
}

}
