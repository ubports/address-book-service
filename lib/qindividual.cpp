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

#include "qindividual.h"
#include "detail-context-parser.h"
#include "gee-utils.h"
#include "update-contact-request.h"

#include "common/vcard-parser.h"

#include <QtVersit/QVersitDocument>
#include <QtVersit/QVersitProperty>
#include <QtVersit/QVersitWriter>
#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactImporter>
#include <QtVersit/QVersitContactExporter>

#include <QtContacts/QContactName>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactBirthday>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactAvatar>
#include <QtContacts/QContactOrganization>
#include <QtContacts/QContactEmailAddress>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactAddress>
#include <QtContacts/QContactOnlineAccount>
#include <QtContacts/QContactUrl>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactFavorite>
#include <QtContacts/QContactGender>
#include <QtContacts/QContactNote>
#include <QtContacts/QContactExtendedDetail>

using namespace QtVersit;
using namespace QtContacts;

namespace
{
static void gValueGeeSetAddStringFieldDetails(GValue *value,
                                              GType g_type,
                                              const char* v_string,
                                              const QtContacts::QContactDetail &detail,
                                              bool ispreferred)
{
    GeeCollection *collection = (GeeCollection*) g_value_get_object(value);
    if(collection == NULL) {
        collection = GEE_COLLECTION(SET_AFD_NEW());
        g_value_take_object(value, collection);
    }

    FolksAbstractFieldDetails *fieldDetails = NULL;

    if(FALSE) {
    } else if(g_type == FOLKS_TYPE_EMAIL_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_email_field_details_new(v_string, NULL));
    } else if(g_type == FOLKS_TYPE_IM_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_im_field_details_new(v_string, NULL));
    } else if(g_type == FOLKS_TYPE_NOTE_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_note_field_details_new(v_string, NULL, NULL));
    } else if(g_type == FOLKS_TYPE_PHONE_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_phone_field_details_new(v_string, NULL));
    } else if(g_type == FOLKS_TYPE_URL_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_url_field_details_new(v_string, NULL));
    } else if(g_type == FOLKS_TYPE_WEB_SERVICE_FIELD_DETAILS) {
        fieldDetails = FOLKS_ABSTRACT_FIELD_DETAILS (
                folks_web_service_field_details_new(v_string, NULL));
    }

    if (fieldDetails == NULL) {
        qWarning() << "Invalid fieldDetails type" << g_type;
    } else {
        galera::DetailContextParser::parseContext(fieldDetails, detail, ispreferred);
        gee_collection_add(collection, fieldDetails);

        g_object_unref(fieldDetails);
    }
}

#define PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(\
        details, cDetails, key, value, q_type, g_type, member, prefDetail) \
{ \
    if(cDetails.size() > 0) { \
        value = GeeUtils::gValueSliceNew(G_TYPE_OBJECT); \
        Q_FOREACH(const q_type& detail, cDetails) { \
            if(!detail.isEmpty()) { \
                gValueGeeSetAddStringFieldDetails(value, (g_type), \
                        detail.member().toUtf8().data(), \
                        detail, \
                        detail == prefDetail); \
            } \
        } \
        GeeUtils::personaDetailsInsert((details), (key), (value)); \
    } \
}

}

namespace galera
{

QIndividual::QIndividual(FolksIndividual *individual, FolksIndividualAggregator *aggregator)
    : m_individual(individual),
      m_primaryPersona(0),
      m_aggregator(aggregator),
      m_contact(0)
{
    g_object_ref(m_individual);
    updateContact();
}

void QIndividual::notifyUpdate()
{
    for(int i=0; i < m_listeners.size(); i++) {
        QPair<QObject*, QMetaMethod> listener = m_listeners[i];
        listener.second.invoke(listener.first, Q_ARG(QIndividual*, this));
    }
}

QIndividual::~QIndividual()
{
    if (m_contact) {
        delete m_contact;
    }
    Q_ASSERT(m_individual);
    g_object_unref(m_individual);
}

QString QIndividual::id() const
{
    if (m_individual) {
        return QString::fromUtf8(folks_individual_get_id(m_individual));
    } else {
        return "";
    }
}

QtContacts::QContactDetail QIndividual::getUid() const
{
    QContactGuid uid;
    const char* id = folks_individual_get_id(m_individual);
    Q_ASSERT(id);
    uid.setGuid(QString::fromUtf8(id));

    return uid;
}

QList<QtContacts::QContactDetail> QIndividual::getClientPidMap() const
{
    QList<QtContacts::QContactDetail> details;
    int index = 1;
    GeeSet *personas = folks_individual_get_personas(m_individual);
    if (!personas) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    while(gee_iterator_next(iter)) {
        QContactExtendedDetail detail;
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
        detail.setName("CLIENTPIDMAP");
        detail.setValue(QContactExtendedDetail::FieldData, index++);
        detail.setValue(QContactExtendedDetail::FieldData + 1, QString::fromUtf8(folks_persona_get_uid(persona)));
        details << detail;
        g_object_unref(persona);
    }

    g_object_unref(iter);
    return details;
}

void QIndividual::appendDetailsForPersona(QtContacts::QContact *contact,
                                          const QtContacts::QContactDetail &detail,
                                          bool readOnly) const
{
    if (!detail.isEmpty()) {
        QtContacts::QContactDetail cpy(detail);
        QtContacts::QContactDetail::AccessConstraints access;
        if (readOnly ||
            detail.accessConstraints().testFlag(QContactDetail::ReadOnly)) {
            access |= QContactDetail::ReadOnly;
        }

        if (detail.accessConstraints().testFlag(QContactDetail::Irremovable)) {
            access |= QContactDetail::Irremovable;
        }

        QContactManagerEngine::setDetailAccessConstraints(&cpy, access);
        contact->appendDetail(cpy);
    }
}

void QIndividual::appendDetailsForPersona(QtContacts::QContact *contact,
                                          QList<QtContacts::QContactDetail> details,
                                          const QString &preferredAction,
                                          const QtContacts::QContactDetail &preferred,
                                          bool readOnly) const
{
    Q_FOREACH(const QContactDetail &detail, details) {
        appendDetailsForPersona(contact, detail, readOnly);
        if (!preferred.isEmpty()) {
            contact->setPreferredDetail(preferredAction, preferred);
        }
    }
}

QContactDetail QIndividual::getPersonaName(FolksPersona *persona, int index) const
{
    if (!FOLKS_IS_NAME_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactName detail;
    FolksStructuredName *sn = folks_name_details_get_structured_name(FOLKS_NAME_DETAILS(persona));
    if (sn) {
        const char *name = folks_structured_name_get_given_name(sn);
        if (name && strlen(name)) {
            detail.setFirstName(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_additional_names(sn);
        if (name && strlen(name)) {
            detail.setMiddleName(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_family_name(sn);
        if (name && strlen(name)) {
            detail.setLastName(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_prefixes(sn);
        if (name && strlen(name)) {
            detail.setPrefix(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_suffixes(sn);
        if (name && strlen(name)) {
            detail.setSuffix(QString::fromUtf8(name));
        }
        detail.setDetailUri(QString("%1.1").arg(index));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaFullName(FolksPersona *persona, int index) const
{
    if (!FOLKS_IS_NAME_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactDisplayLabel detail;
    FolksStructuredName *sn = folks_name_details_get_structured_name(FOLKS_NAME_DETAILS(persona));
    QString displayName;
    if (sn) {
        const char *name = folks_structured_name_get_given_name(sn);
        if (name && strlen(name)) {
            displayName += QString::fromUtf8(name);
        }
        name = folks_structured_name_get_family_name(sn);
        if (name && strlen(name)) {
            if (!displayName.isEmpty()) {
                displayName += " ";
            }
            displayName += QString::fromUtf8(name);
        }
    }
    if (!displayName.isEmpty()) {
        detail.setLabel(displayName);
        detail.setDetailUri(QString("%1.1").arg(index));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaNickName(FolksPersona *persona, int index) const
{
    if (!FOLKS_IS_NAME_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactNickname detail;
    const gchar* nickname = folks_name_details_get_nickname(FOLKS_NAME_DETAILS(persona));
    if (nickname && strlen(nickname)) {
        detail.setNickname(QString::fromUtf8(nickname));
        detail.setDetailUri(QString("%1.1").arg(index));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaBirthday(FolksPersona *persona, int index) const
{
    if (!FOLKS_IS_BIRTHDAY_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactBirthday detail;
    GDateTime* datetime = folks_birthday_details_get_birthday(FOLKS_BIRTHDAY_DETAILS(persona));
    if (datetime) {
        QDate date(g_date_time_get_year(datetime), g_date_time_get_month(datetime), g_date_time_get_day_of_month(datetime));
        QTime time(g_date_time_get_hour(datetime), g_date_time_get_minute(datetime), g_date_time_get_second(datetime));
        detail.setDateTime(QDateTime(date, time));
        detail.setDetailUri(QString("%1.1").arg(index));
    }
    return detail;
}

void QIndividual::folksPersonaChanged(FolksPersona *persona,
                                      GParamSpec *pspec,
                                      QIndividual *self)
{
    const gchar* paramName = g_param_spec_get_name(pspec);
    if (QString::fromUtf8(paramName) == QStringLiteral("avatar")) {
        QContactDetail newAvatar = self->getPersonaPhoto(persona, 1);

        QContactAvatar avatar = self->m_contact->detail(QContactAvatar::Type);
        avatar.setImageUrl(newAvatar.value(QContactAvatar::FieldImageUrl).toUrl());

        self->m_contact->saveDetail(&avatar);
        self->notifyUpdate();
    }
    //TODO: implement for all details
}

QtContacts::QContactDetail QIndividual::getPersonaPhoto(FolksPersona *persona, int index) const
{
    QContactAvatar avatar;
    if (!FOLKS_IS_AVATAR_DETAILS(persona)) {
        return avatar;
    }

    GLoadableIcon *avatarIcon = folks_avatar_details_get_avatar(FOLKS_AVATAR_DETAILS(persona));
    if (avatarIcon) {
        QString url;
        if (G_IS_FILE_ICON(avatarIcon)) {
            GFile *avatarFile = g_file_icon_get_file(G_FILE_ICON(avatarIcon));
            gchar *uri = g_file_get_uri(avatarFile);
            if (uri) {
                url = QString::fromUtf8(uri);
                g_free(uri);
            }
        } else {
            FolksAvatarCache *cache = folks_avatar_cache_dup();
            const char *contactId = folks_individual_get_id(m_individual);
            gchar *uri = folks_avatar_cache_build_uri_for_avatar(cache, contactId);
            url = QString::fromUtf8(uri);
            if (!QFile::exists(url)) {
                folks_avatar_cache_store_avatar(cache,
                                                contactId,
                                                avatarIcon,
                                                QIndividual::avatarCacheStoreDone,
                                                strdup(uri));
            }
            g_free(uri);
            g_object_unref(cache);
        }
        avatar.setImageUrl(QUrl(url));
    }
    avatar.setDetailUri(QString("%1.1").arg(index));
    return avatar;
}

void QIndividual::avatarCacheStoreDone(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = 0;
    gchar *uri = folks_avatar_cache_store_avatar_finish(FOLKS_AVATAR_CACHE(source),
                                                        result,
                                                        &error);

    if (error) {
        qWarning() << "Fail to store avatar" << error->message;
        g_error_free(error);
    }

    Q_ASSERT(g_str_equal(data, uri));
    g_free(data);
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaRoles(FolksPersona *persona,
                                                               QtContacts::QContactDetail *preferredRole,
                                                               int index) const
{
    if (!FOLKS_IS_ROLE_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *roles = folks_role_details_get_roles(FOLKS_ROLE_DETAILS(persona));
    if (!roles) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(roles));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        FolksRole *role = FOLKS_ROLE(folks_abstract_field_details_get_value(fd));
        QContactOrganization org;
        QString field;

        field = QString::fromUtf8(folks_role_get_organisation_name(role));
        if (!field.isEmpty()) {
            org.setName(field);
        }
        field = QString::fromUtf8(folks_role_get_title(role));
        if (!field.isEmpty()) {
            org.setTitle(field);
        }
        field = QString::fromUtf8(folks_role_get_role(role));
        if (!field.isEmpty()) {
            org.setRole(field);
        }
        bool isPref = false;
        DetailContextParser::parseParameters(org, fd, &isPref);
        org.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
        if (isPref) {
            *preferredRole = org;
        }

        g_object_unref(fd);
        details << org;
    }
    g_object_unref(iter);
    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaEmails(FolksPersona *persona,
                                                                QtContacts::QContactDetail *preferredEmail,
                                                                int index) const
{
    if (!FOLKS_IS_EMAIL_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *emails = folks_email_details_get_email_addresses(FOLKS_EMAIL_DETAILS(persona));
    if (!emails) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(emails));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));

        const gchar *email = (const gchar*) folks_abstract_field_details_get_value(fd);

        QContactEmailAddress addr;
        addr.setEmailAddress(QString::fromUtf8(email));
        bool isPref = false;
        DetailContextParser::parseParameters(addr, fd, &isPref);
        addr.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
        if (isPref) {
            *preferredEmail = addr;
        }

        g_object_unref(fd);
        details << addr;
    }
    g_object_unref(iter);
    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaPhones(FolksPersona *persona,
                                                                QtContacts::QContactDetail *preferredPhone,
                                                                int index) const
{
    if (!FOLKS_IS_PHONE_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *phones = folks_phone_details_get_phone_numbers(FOLKS_PHONE_DETAILS(persona));
    if (!phones) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(phones));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        const gchar *phone = (const char*) folks_abstract_field_details_get_value(fd);

        QContactPhoneNumber number;
        number.setNumber(QString::fromUtf8(phone));
        bool isPref = false;
        DetailContextParser::parseParameters(number, fd, &isPref);
        number.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
        if (isPref) {
            *preferredPhone = number;
        }

        g_object_unref(fd);
        details << number;
    }
    g_object_unref(iter);
    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaAddresses(FolksPersona *persona,
                                                                   QtContacts::QContactDetail *preferredAddress,
                                                                   int index) const
{
    if (!FOLKS_IS_POSTAL_ADDRESS_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *addresses = folks_postal_address_details_get_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(persona));
    if (!addresses) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(addresses));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        FolksPostalAddress *addr = FOLKS_POSTAL_ADDRESS(folks_abstract_field_details_get_value(fd));

        QContactAddress address;
        const char *field = folks_postal_address_get_country(addr);
        if (field && strlen(field)) {
            address.setCountry(QString::fromUtf8(field));
        }

        field = folks_postal_address_get_locality(addr);
        if (field && strlen(field)) {
            address.setLocality(QString::fromUtf8(field));
        }

        field = folks_postal_address_get_po_box(addr);
        if (field && strlen(field)) {
            address.setPostOfficeBox(QString::fromUtf8(field));
        }

        field = folks_postal_address_get_postal_code(addr);
        if (field && strlen(field)) {
            address.setPostcode(QString::fromUtf8(field));
        }

        field = folks_postal_address_get_region(addr);
        if (field && strlen(field)) {
            address.setRegion(QString::fromUtf8(field));
        }

        field = folks_postal_address_get_street(addr);
        if (field && strlen(field)) {
            address.setStreet(QString::fromUtf8(field));
        }

        bool isPref = false;
        DetailContextParser::parseParameters(address, fd, &isPref);
        address.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
        if (isPref) {
            *preferredAddress = address;
        }

        g_object_unref(fd);
        details << address;
    }
    g_object_unref(iter);
    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaIms(FolksPersona *persona,
                                                             QtContacts::QContactDetail *preferredIm,
                                                             int index) const
{
    if (!FOLKS_IS_IM_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeMultiMap *ims = folks_im_details_get_im_addresses(FOLKS_IM_DETAILS(persona));
    if (!ims) {
        return details;
    }
    GeeSet *keys = gee_multi_map_get_keys(ims);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(keys));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        const gchar *key = (const gchar*) gee_iterator_get(iter);
        GeeCollection *values = gee_multi_map_get(ims, key);

        GeeIterator *iterValues = gee_iterable_iterator(GEE_ITERABLE(values));
        while(gee_iterator_next(iterValues)) {
            FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iterValues));
            const char *uri = (const char*) folks_abstract_field_details_get_value(fd);

            QContactOnlineAccount account;
            account.setAccountUri(QString::fromUtf8(uri));
            int protocolId = DetailContextParser::accountProtocolFromString(QString::fromUtf8(key));
            account.setProtocol(static_cast<QContactOnlineAccount::Protocol>(protocolId));

            bool isPref = false;
            DetailContextParser::parseParameters(account, fd, &isPref);
            account.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
            if (isPref) {
                *preferredIm = account;
            }
            g_object_unref(fd);
            details << account;
        }
        g_object_unref(iterValues);
    }

    g_object_unref(iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaUrls(FolksPersona *persona,
                                                              QtContacts::QContactDetail *preferredUrl,
                                                              int index) const
{
    if (!FOLKS_IS_URL_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *urls = folks_url_details_get_urls(FOLKS_URL_DETAILS(persona));
    if (!urls) {
        return details;
    }
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(urls));
    int fieldIndex = 1;

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        const char *url = (const char*) folks_abstract_field_details_get_value(fd);

        QContactUrl detail;
        detail.setUrl(QString::fromUtf8(url));
        bool isPref = false;
        DetailContextParser::parseParameters(detail, fd, &isPref);
        detail.setDetailUri(QString("%1.%2").arg(index).arg(fieldIndex++));
        if (isPref) {
            *preferredUrl = detail;
        }
        g_object_unref(fd);
        details << detail;
    }
    g_object_unref(iter);
    return details;
}

QtContacts::QContactDetail QIndividual::getPersonaFavorite(FolksPersona *persona, int index) const
{
    if (!FOLKS_IS_FAVOURITE_DETAILS(persona)) {
        return QtContacts::QContactDetail();
    }

    QContactFavorite detail;
    detail.setFavorite(folks_favourite_details_get_is_favourite(FOLKS_FAVOURITE_DETAILS(persona)));
    detail.setDetailUri(QString("%1.%2").arg(index).arg(1));
    return detail;
}

QtContacts::QContact QIndividual::copy(QList<QContactDetail::DetailType> fields)
{
    QList<QContactDetail> details;
    QContact result;


    if (fields.isEmpty()) {
        result = contact();
    } else {
        QContact fullContact = contact();
        // this will remove the contact details but will keep the other metadata like preferred fields
        result = fullContact;
        result.clearDetails();

        // mandatory
        details << fullContact.detail<QContactGuid>();
        Q_FOREACH(QContactDetail det, fullContact.details<QContactExtendedDetail>()) {
            details << det;
        }

        if (fields.contains(QContactDetail::TypeName)) {
            details << fullContact.detail<QContactName>();
        }


        if (fields.contains(QContactDetail::TypeDisplayLabel)) {
            details << fullContact.detail<QContactDisplayLabel>();
        }

        if (fields.contains(QContactDetail::TypeNickname)) {
            details << fullContact.detail<QContactNickname>();
        }

        if (fields.contains(QContactDetail::TypeBirthday)) {
            details << fullContact.detail<QContactBirthday>();
        }

        if (fields.contains(QContactDetail::TypeAvatar)) {
            details << fullContact.detail<QContactAvatar>();
        }

        if (fields.contains(QContactDetail::TypeOrganization)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactOrganization>()) {
                details << det;
            }
        }

        if (fields.contains(QContactDetail::TypeEmailAddress)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactEmailAddress>()) {
                details << det;
            }
        }

        if (fields.contains(QContactDetail::TypePhoneNumber)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactPhoneNumber>()) {
                details << det;
            }
        }

        if (fields.contains(QContactDetail::TypeAddress)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactAddress>()) {
                details << det;
            }
        }

        if (fields.contains(QContactDetail::TypeUrl)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactUrl>()) {
                details << det;
            }
        }

        Q_FOREACH(QContactDetail det, details) {
            result.appendDetail(det);
        }
    }

    return result;
}

QtContacts::QContact &QIndividual::contact()
{
    if (!m_contact) {
        updateContact();
    }
    return *m_contact;
}

void QIndividual::updateContact()
{
    Q_ASSERT(m_individual);

    m_contact = new QContact();
    m_contact->appendDetail(getUid());
    Q_FOREACH(QContactDetail detail, getClientPidMap()) {
        m_contact->appendDetail(detail);
    }

    int personaIndex = 1;
    GeeSet *personas = folks_individual_get_personas(m_individual);
    Q_ASSERT(personas);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    FolksPersona *persona;

    // disconnect any previous handler
    Q_FOREACH(gpointer persona, m_notifyConnections.keys()) {
        Q_FOREACH(int handlerId, m_notifyConnections[persona]) {
            g_signal_handler_disconnect(persona, handlerId);
        }
    }

    m_notifyConnections.clear();

    while(gee_iterator_next(iter)) {
         persona = FOLKS_PERSONA(gee_iterator_get(iter));
         Q_ASSERT(FOLKS_IS_PERSONA(persona));

        int wsize = 0;
        gchar **wproperties = folks_persona_get_writeable_properties(persona, &wsize);
        //"gender", "local-ids", "avatar", "postal-addresses", "urls", "phone-numbers", "structured-name",
        //"anti-links", "im-addresses", "is-favourite", "birthday", "notes", "roles", "email-addresses",
        //"web-service-addresses", "groups", "full-name"
        QStringList wPropList;
        for(int i=0; i < wsize; i++) {
            wPropList << wproperties[i];
        }

        appendDetailsForPersona(m_contact,
                                getPersonaName(persona, personaIndex),
                                !wPropList.contains("structured-name"));
        appendDetailsForPersona(m_contact,
                                getPersonaFullName(persona, personaIndex),
                                !wPropList.contains("full-name"));
        appendDetailsForPersona(m_contact,
                                getPersonaNickName(persona, personaIndex),
                                !wPropList.contains("structured-name"));
        appendDetailsForPersona(m_contact,
                                getPersonaBirthday(persona, personaIndex),
                                !wPropList.contains("birthday"));
        appendDetailsForPersona(m_contact,
                                getPersonaPhoto(persona, personaIndex),
                                !wPropList.contains("avatar"));
        appendDetailsForPersona(m_contact,
                                getPersonaFavorite(persona, personaIndex),
                                !wPropList.contains("is-favourite"));

        QList<QContactDetail> details;
        QContactDetail prefDetail;
        details = getPersonaRoles(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactOrganization::Type],
                                prefDetail,
                                !wPropList.contains("roles"));

        details = getPersonaEmails(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactEmailAddress::Type],
                                prefDetail,
                                !wPropList.contains("email-addresses"));

        details = getPersonaPhones(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactPhoneNumber::Type],
                                prefDetail,
                                !wPropList.contains("phone-numbers"));

        details = getPersonaAddresses(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactAddress::Type],
                                prefDetail,
                                !wPropList.contains("postal-addresses"));

        details = getPersonaIms(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactOnlineAccount::Type],
                                prefDetail,
                                !wPropList.contains("im-addresses"));

        details = getPersonaUrls(persona, &prefDetail, personaIndex);
        appendDetailsForPersona(m_contact,
                                details,
                                VCardParser::PreferredActionNames[QContactUrl::Type],
                                prefDetail,
                                !wPropList.contains("urls"));
        personaIndex++;

        // for now we are getting updated only about avatar changes
        int handlerId = g_signal_connect(G_OBJECT(persona), "notify::avatar",
                                         (GCallback) QIndividual::folksPersonaChanged,
                                         const_cast<QIndividual*>(this));
        QList<int> ids = m_notifyConnections[persona];
        ids << handlerId;
        m_notifyConnections[persona] = ids;

        g_object_unref(persona);
    }

    g_object_unref(iter);
}

bool QIndividual::update(const QtContacts::QContact &newContact, QObject *object, const char *slot)
{
    QContact &originalContact = contact();
    if (newContact != originalContact) {
        UpdateContactRequest *request = new UpdateContactRequest(newContact, this, object, slot);

        QObject::connect(request, &UpdateContactRequest::done, [request, this] (const QString &errorMessage) {
            if (errorMessage.isEmpty()) {
                this->updateContact();
            }
            request->deleteLater();
        });
        request->start();
        return true;
    } else {
        qDebug() << "Contact is equal";
        return false;
    }
}

bool QIndividual::update(const QString &vcard, QObject *object, const char *slot)
{
    QContact contact = VCardParser::vcardToContact(vcard);
    return update(contact, object, slot);
}


FolksIndividual *QIndividual::individual() const
{
    return m_individual;
}

FolksPersona *QIndividual::getPersona(int index) const
{
    int size = 0;
    FolksPersona *persona = 0;
    GeeSet *personas = folks_individual_get_personas(m_individual);
    gpointer* values = gee_collection_to_array(GEE_COLLECTION(personas), &size);

    if ((index > 0) && (index <= size)) {
        persona = FOLKS_PERSONA(values[index - 1]);
    }

    g_free(values);
    return persona;
}

int QIndividual::personaCount() const
{
    GeeSet *personas = folks_individual_get_personas(m_individual);
    return gee_collection_get_size(GEE_COLLECTION(personas));
}

void QIndividual::addListener(QObject *object, const char *slot)
{
    int slotIndex = object->metaObject()->indexOfSlot(++slot);
    if (slotIndex == -1) {
        qWarning() << "Invalid slot:" << slot << "for object" << object;
    } else {
        m_listeners << qMakePair(object, object->metaObject()->method(slotIndex));
    }
}

void QIndividual::setIndividual(FolksIndividual *individual)
{
    if (m_individual != individual) {
        if (m_individual) {
            g_object_unref(m_individual);
        }
        m_individual = individual;
        if (m_individual) {
            g_object_ref(m_individual);
        }

        // initialize qcontact
        if (m_contact) {
            delete m_contact;
            m_contact = 0;
        }
        updateContact();
    }
}


GHashTable *QIndividual::parseAddressDetails(GHashTable *details,
                                             const QList<QtContacts::QContactDetail> &cDetails,
                                             const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = GeeUtils::gValueSliceNew(G_TYPE_OBJECT);

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        if(!detail.isEmpty()) {
            QContactAddress address = static_cast<QContactAddress>(detail);
            FolksPostalAddress *postalAddress = folks_postal_address_new(address.postOfficeBox().toUtf8().data(),
                                                                         NULL, // extension
                                                                         address.street().toUtf8().data(),
                                                                         address.locality().toUtf8().data(),
                                                                         address.region().toUtf8().data(),
                                                                         address.postcode().toUtf8().data(),
                                                                         address.country().toUtf8().data(),
                                                                         NULL,  // address format
                                                                         NULL); //UID

            GeeCollection *collection = (GeeCollection*) g_value_get_object(value);
            if(collection == NULL) {
                collection = GEE_COLLECTION(SET_AFD_NEW());
                g_value_take_object(value, collection);
            }

            FolksPostalAddressFieldDetails *pafd = folks_postal_address_field_details_new(postalAddress, NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(pafd),
                                              address,
                                              detail == prefDetail);
            gee_collection_add(collection, pafd);

            g_object_unref(pafd);
            g_object_unref(postalAddress);
        }
        GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_POSTAL_ADDRESSES, value);
    }

    return details;
}

GHashTable *QIndividual::parsePhotoDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactAvatar avatar = static_cast<QContactAvatar>(detail);
        if(!avatar.isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(G_TYPE_FILE_ICON);
            QUrl avatarUri = avatar.imageUrl();
            if(!avatarUri.isEmpty()) {
                QString formattedUri = avatarUri.toString(QUrl::RemoveUserInfo);
                if(!formattedUri.isEmpty()) {
                    GFile *avatarFile = g_file_new_for_uri(formattedUri.toUtf8().data());
                    GFileIcon *avatarFileIcon = G_FILE_ICON(g_file_icon_new(avatarFile));
                    g_value_take_object(value, avatarFileIcon);

                    GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_AVATAR, value);
                    g_clear_object((GObject**) &avatarFile);
                }
            } else {
                g_object_unref(value);
            }
        }
    }

    return details;
}

GHashTable *QIndividual::parseBirthdayDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactBirthday birthday = static_cast<QContactBirthday>(detail);
        if(!birthday.isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(G_TYPE_DATE_TIME);
            GDateTime *dateTime = g_date_time_new_from_unix_utc(birthday.dateTime().toMSecsSinceEpoch() / 1000);
            g_value_set_boxed(value, dateTime);

            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_BIRTHDAY, value);
            g_date_time_unref(dateTime);
        }
    }

    return details;
}

GHashTable *QIndividual::parseEmailDetails(GHashTable *details,
                                           const QList<QtContacts::QContactDetail> &cDetails,
                                           const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_EMAIL_ADDRESSES, value, QContactEmailAddress,
                                                FOLKS_TYPE_EMAIL_FIELD_DETAILS, emailAddress, prefDetail);
    return details;
}

GHashTable *QIndividual::parseFavoriteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactFavorite favorite = static_cast<QContactFavorite>(detail);
        if(!favorite.isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(G_TYPE_BOOLEAN);
            g_value_set_boolean(value, favorite.isFavorite());

            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IS_FAVOURITE, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseGenderDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactGender gender = static_cast<QContactDetail>(detail);
        if(!gender.isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(FOLKS_TYPE_GENDER);
            FolksGender genderEnum = FOLKS_GENDER_UNSPECIFIED;
            if(gender.gender() == QContactGender::GenderMale) {
                genderEnum = FOLKS_GENDER_MALE;
            } else if(gender.gender() == QContactGender::GenderFemale) {
                genderEnum = FOLKS_GENDER_FEMALE;
            }
            g_value_set_enum(value, genderEnum);

            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_GENDER, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactName name = static_cast<QContactName>(detail);
        if(!name.isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(FOLKS_TYPE_STRUCTURED_NAME);
            FolksStructuredName *sn = folks_structured_name_new(name.lastName().toUtf8().data(),
                                                                name.firstName().toUtf8().data(),
                                                                name.middleName().toUtf8().data(),
                                                                name.prefix().toUtf8().data(),
                                                                name.suffix().toUtf8().data());
            g_value_take_object(value, sn);
            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_STRUCTURED_NAME, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseFullNameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactDisplayLabel displayLabel = static_cast<QContactDisplayLabel>(detail);
        if(!displayLabel.label().isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, displayLabel.label().toUtf8().data());
            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_FULL_NAME, value);

            // FIXME: check if those values should all be set to the same thing
            value = GeeUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, displayLabel.label().toUtf8().data());
            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ALIAS, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseNicknameDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactNickname nickname = static_cast<QContactNickname>(detail);
        if(!nickname.nickname().isEmpty()) {
            GValue *value = GeeUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, nickname.nickname().toUtf8().data());
            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_NICKNAME, value);

            // FIXME: check if those values should all be set to the same thing
            value = GeeUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, nickname.nickname().toUtf8().data());
            GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ALIAS, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseNoteDetails(GHashTable *details,
                                          const QList<QtContacts::QContactDetail> &cDetails,
                                          const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_NOTES, value, QContactNote,
                                                FOLKS_TYPE_NOTE_FIELD_DETAILS, note, prefDetail);

    return details;
}

GHashTable *QIndividual::parseImDetails(GHashTable *details,
                                        const QList<QtContacts::QContactDetail> &cDetails,
                                        const QtContacts::QContactDetail &prefDetail)
{
    Q_UNUSED(prefDetail);

    if(cDetails.size() == 0) {
        return details;
    }

    QMultiMap<QString, QString> providerUidMap;
    Q_FOREACH(const QContactDetail &detail, cDetails) {
        QContactOnlineAccount account = static_cast<QContactOnlineAccount>(detail);
        if (!account.isEmpty()) {
            providerUidMap.insert(DetailContextParser::accountProtocolName(account.protocol()), account.accountUri());
        }
    }

    if(!providerUidMap.isEmpty()) {
        //TODO: add account type and subtype
        GValue *value = GeeUtils::asvSetStrNew(providerUidMap);
        GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IM_ADDRESSES, value);
    }

    return details;
}

GHashTable *QIndividual::parseOrganizationDetails(GHashTable *details,
                                                  const QList<QtContacts::QContactDetail> &cDetails,
                                                  const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = GeeUtils::gValueSliceNew(G_TYPE_OBJECT);
    Q_FOREACH(const QContactDetail& detail, cDetails) {
        QContactOrganization org = static_cast<QContactOrganization>(detail);
        if(!org.isEmpty()) {
            FolksRole *role = folks_role_new(org.title().toUtf8().data(),
                                             org.name().toUtf8().data(),
                                             NULL);
            folks_role_set_role(role, org.role().toUtf8().data());

            GeeCollection *collection = (GeeCollection*) g_value_get_object(value);
            if(collection == NULL) {
                collection = GEE_COLLECTION(SET_AFD_NEW());
                g_value_take_object(value, collection);
            }
            FolksRoleFieldDetails *rfd = folks_role_field_details_new(role, NULL);
            DetailContextParser::parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(rfd), org, detail == prefDetail);
            gee_collection_add(collection, rfd);

            g_object_unref(rfd);
            g_object_unref(role);
        }
    }
    GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ROLES, value);

    return details;
}

GHashTable *QIndividual::parsePhoneNumbersDetails(GHashTable *details,
                                                  const QList<QtContacts::QContactDetail> &cDetails,
                                                  const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = GeeUtils::gValueSliceNew(G_TYPE_OBJECT);
    Q_FOREACH(const QContactDetail &detail, cDetails) {
        QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(detail);
        if(!phone.isEmpty()) {
            gValueGeeSetAddStringFieldDetails(value,
                                              FOLKS_TYPE_PHONE_FIELD_DETAILS,
                                              phone.number().toUtf8().data(),
                                              phone, detail == prefDetail);
        }
    }
    GeeUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_PHONE_NUMBERS, value);

    return details;
}

GHashTable *QIndividual::parseUrlDetails(GHashTable *details,
                                         const QList<QtContacts::QContactDetail> &cDetails,
                                         const QtContacts::QContactDetail &prefDetail)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_URLS, value, QContactUrl,
                                                FOLKS_TYPE_URL_FIELD_DETAILS, url, prefDetail);

    return details;
}


GHashTable *QIndividual::parseDetails(const QtContacts::QContact &contact)
{
    GHashTable *details = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) GeeUtils::gValueSliceFree);

    parsePhotoDetails(details, contact.details(QContactAvatar::Type));
    parseBirthdayDetails(details, contact.details(QContactBirthday::Type));
    parseFavoriteDetails(details, contact.details(QContactFavorite::Type));
    parseGenderDetails(details, contact.details(QContactGender::Type));
    parseNameDetails(details, contact.details(QContactName::Type));
    parseFullNameDetails(details, contact.details(QContactDisplayLabel::Type));
    parseNicknameDetails(details, contact.details(QContactNickname::Type));


    parseAddressDetails(details,
                        contact.details(QContactAddress::Type),
                        contact.preferredDetail(VCardParser::PreferredActionNames[QContactAddress::Type]));
    parseEmailDetails(details,
                      contact.details(QContactEmailAddress::Type),
                      contact.preferredDetail(VCardParser::PreferredActionNames[QContactEmailAddress::Type]));
    parseNoteDetails(details,
                     contact.details(QContactNote::Type),
                     contact.preferredDetail(VCardParser::PreferredActionNames[QContactNote::Type]));
    parseImDetails(details,
                   contact.details(QContactOnlineAccount::Type),
                   contact.preferredDetail(VCardParser::PreferredActionNames[QContactOnlineAccount::Type]));
    parseOrganizationDetails(details,
                             contact.details(QContactOrganization::Type),
                             contact.preferredDetail(VCardParser::PreferredActionNames[QContactOrganization::Type]));
    parsePhoneNumbersDetails(details,
                             contact.details(QContactPhoneNumber::Type),
                             contact.preferredDetail(VCardParser::PreferredActionNames[QContactPhoneNumber::Type]));
    parseUrlDetails(details,
                    contact.details(QContactUrl::Type),
                    contact.preferredDetail(VCardParser::PreferredActionNames[QContactUrl::Type]));

    return details;
}

FolksPersona* QIndividual::primaryPersona()
{
    Q_ASSERT(m_individual);

    if (m_primaryPersona) {
        return m_primaryPersona;
    }

    GeeSet *personas = folks_individual_get_personas(m_individual);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));
    FolksPersonaStore *primaryStore = folks_individual_aggregator_get_primary_store(m_aggregator);

    while(m_primaryPersona == NULL && gee_iterator_next(iter)) {
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
        if(folks_persona_get_store(persona) == primaryStore) {
            m_primaryPersona = persona;
            g_object_ref (persona);
        }
        g_object_unref(persona);
    }
    g_object_unref(iter);

    return m_primaryPersona;
}

QtContacts::QContactDetail QIndividual::detailFromUri(QtContacts::QContactDetail::DetailType type, const QString &uri) const
{
    Q_FOREACH(QContactDetail detail, m_contact->details(type)) {
        if (detail.detailUri() == uri) {
            return detail;
        }
    }
    return QContactDetail();
}

} //namespace

