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
#include "vcard-parser.h"

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

class UpdateContactData
{
public:
    QContactDetail::DetailType m_currentDetailType;
    QContact m_newContact;
    galera::QIndividual *m_self;
    FolksPersona *m_persona;

    QList<QContactDetail> m_detailsChanged;
    galera::UpdateDoneCB m_doneCB;
    void *m_doneData;
};

#ifdef FOLKS_0_9_0
static guint _folks_abstract_field_details_hash_data_func (gconstpointer v, gpointer self)
{
    const FolksAbstractFieldDetails *constDetails = static_cast<const FolksAbstractFieldDetails*>(v);
    return folks_abstract_field_details_hash_static (const_cast<FolksAbstractFieldDetails*>(constDetails));
}

static int _folks_abstract_field_details_equal_data_func (gconstpointer a, gconstpointer b, gpointer self)
{
    const FolksAbstractFieldDetails *constDetailsA = static_cast<const FolksAbstractFieldDetails*>(a);
    const FolksAbstractFieldDetails *constDetailsB = static_cast<const FolksAbstractFieldDetails*>(b);
    return folks_abstract_field_details_equal_static (const_cast<FolksAbstractFieldDetails*>(constDetailsA), const_cast<FolksAbstractFieldDetails*>(constDetailsB));
}
#endif

}
namespace galera
{

// TODO: find a better way to link abstract field with details
#define FOLKS_DATA_FIELD        10000

#ifdef FOLKS_0_9_0
    #define SET_AFD_NEW() \
        GEE_SET(gee_hash_set_new(FOLKS_TYPE_ABSTRACT_FIELD_DETAILS, \
                                 (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                                 _folks_abstract_field_details_hash_data_func, \
                                 NULL, \
                                 NULL, \
                                 _folks_abstract_field_details_equal_data_func, \
                                 NULL, \
                                 NULL))

    #define GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE) \
        GEE_MULTI_MAP(gee_hash_multi_map_new(G_TYPE_STRING,\
                                             (GBoxedCopyFunc) g_strdup, g_free, \
                                             FOLKS_TYPE, \
                                             (GBoxedCopyFunc)g_object_ref, g_object_unref, \
                                             NULL, \
                                             NULL, \
                                             NULL, \
                                             NULL, \
                                             NULL, \
                                             NULL, \
                                             _folks_abstract_field_details_hash_data_func, \
                                             NULL, \
                                             NULL, \
                                             _folks_abstract_field_details_equal_data_func, \
                                             NULL, \
                                             NULL))
#else
    #define SET_AFD_NEW() \
        GEE_SET(gee_hash_set_new(FOLKS_TYPE_ABSTRACT_FIELD_DETAILS, \
                                 (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                                 (GHashFunc) folks_abstract_field_details_hash, \
                                 (GEqualFunc) folks_abstract_field_details_equal))

    #define GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE) \
        GEE_MULTI_MAP(gee_hash_multi_map_new(G_TYPE_STRING, \
                                             (GBoxedCopyFunc) g_strdup, \
                                              g_free, \
                                              FOLKS_TYPE, \
                                              g_object_ref, g_object_unref, \
                                              g_str_hash, \
                                              g_str_equal, \
                                              (GHashFunc) folks_abstract_field_details_hash, \
                                              (GEqualFunc) folks_abstract_field_details_equal));

#endif


#define PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(\
        details, key, value, q_type, g_type, member) \
{ \
    QList<q_type> contactDetails = contact.details<q_type>(); \
    if(contactDetails.size() > 0) { \
        value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT); \
        Q_FOREACH(const q_type& detail, contact.details<q_type>()) { \
            if(!detail.isEmpty()) { \
                QIndividualUtils::gValueGeeSetAddStringFieldDetails(value, (g_type), \
                        detail.member().toUtf8().data(), \
                        detail); \
            } \
        } \
        QIndividualUtils::personaDetailsInsert((details), (key), (value)); \
    } \
}

class QIndividualUtils
{
public:
    static GValue* gValueSliceNew(GType type)
    {
        GValue *retval = g_slice_new0(GValue);
        g_value_init(retval, type);

        return retval;
    }

    static void gValueSliceFree(GValue *value)
    {
        g_value_unset(value);
        g_slice_free(GValue, value);
    }

    static void gValueGeeSetAddStringFieldDetails(GValue *value,
                                                  GType g_type,
                                                  const char* v_string,
                                                  const QContactDetail &detail)
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
            galera::QIndividual::parseContext(fieldDetails, detail);
            gee_collection_add(collection, fieldDetails);

            g_object_unref(fieldDetails);
        }
    }

    static void personaDetailsInsert(GHashTable *details, FolksPersonaDetail key, gpointer value)
    {
        g_hash_table_insert(details, (gpointer) folks_persona_store_detail_key(key), value);
    }

    static GValue* asvSetStrNew(QMultiMap<QString, QString> providerUidMap)
    {
        GeeMultiMap *hashSet = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);
        GValue *retval = gValueSliceNew (G_TYPE_OBJECT);
        g_value_take_object (retval, hashSet);

        QList<QString> keys = providerUidMap.keys();
        Q_FOREACH(const QString& key, keys) {
            QList<QString> values = providerUidMap.values(key);
            Q_FOREACH(const QString& value, values) {
                FolksImFieldDetails *imfd;

                imfd = folks_im_field_details_new (value.toUtf8().data(), NULL);

                gee_multi_map_set(hashSet,
                                  key.toUtf8().data(),
                                  imfd);
                g_object_unref(imfd);
            }
        }

        return retval;
    }

}; // class

QIndividual::QIndividual(FolksIndividual *individual, FolksIndividualAggregator *aggregator)
    : m_individual(individual),
      m_aggregator(aggregator)
{
    g_object_ref(m_individual);
}

QIndividual::~QIndividual()
{
    g_object_unref(m_individual);
}

QtContacts::QContactDetail QIndividual::getUid() const
{
    QContactGuid uid;
    uid.setGuid(QString::fromUtf8(folks_individual_get_id(m_individual)));

    return uid;
}

QList<QtContacts::QContactDetail> QIndividual::getClientPidMap() const
{
    QList<QtContacts::QContactDetail> details;
    int index = 1;

    /*
    GeeSet *personas = folks_individual_get_personas(m_individual);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    while(gee_iterator_next(iter)) {
        QContactExtendedDetail detail;
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
        detail.setName("CLIENTPIDMAP");
        detail.setValue(QContactExtendedDetail::FieldData, index++);
        detail.setValue(QContactExtendedDetail::FieldData + 1, QString::fromUtf8(folks_persona_get_uid(persona)));
        details << detail;
    }

    g_object_unref(iter);
    */
    return details;
}


QtContacts::QContactDetail QIndividual::getName() const
{
    QContactName detail;
    FolksStructuredName *sn = folks_name_details_get_structured_name(FOLKS_NAME_DETAILS(m_individual));

    if (sn) {
        const char *name = folks_structured_name_get_given_name(sn);
        if (name && strlen(name)) {
            detail.setFirstName(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_family_name(sn);
        if (name && strlen(name)) {
            detail.setMiddleName(QString::fromUtf8(name));
        }
        name = folks_structured_name_get_additional_names(sn);
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
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getFullName() const
{
    QContactDisplayLabel detail;
    const gchar *fullName = folks_name_details_get_full_name(FOLKS_NAME_DETAILS(m_individual));
    if (fullName && strlen(fullName)) {
        detail.setLabel(QString::fromUtf8(fullName));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getNickname() const
{
    QContactNickname detail;
    const gchar* nickname = folks_name_details_get_nickname(FOLKS_NAME_DETAILS(m_individual));
    if (nickname && strlen(nickname)) {
        detail.setNickname(QString::fromUtf8(nickname));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getBirthday() const
{
    QContactBirthday detail;
    GDateTime* datetime = folks_birthday_details_get_birthday(FOLKS_BIRTHDAY_DETAILS(m_individual));
    if (datetime) {
        QDate date(g_date_time_get_year(datetime), g_date_time_get_month(datetime), g_date_time_get_day_of_month(datetime));
        QTime time(g_date_time_get_hour(datetime), g_date_time_get_minute(datetime), g_date_time_get_second(datetime));
        detail.setDateTime(QDateTime(date, time));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPhoto() const
{
    // TODO
    return QContactAvatar();
}

QList<QtContacts::QContactDetail> QIndividual::getRoles() const
{
    QList<QtContacts::QContactDetail> details;
    GeeSet *roles = folks_role_details_get_roles(FOLKS_ROLE_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(roles));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        FolksRole *role = FOLKS_ROLE(folks_abstract_field_details_get_value(fd));

        QContactOrganization org;
        org.setName(QString::fromUtf8(folks_role_get_organisation_name(role)));
        org.setTitle(QString::fromUtf8(folks_role_get_title(role)));
        parseParameters(org, fd);

        g_object_unref(fd);
        details << org;
    }

    g_object_unref (iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getEmails() const
{
    QList<QtContacts::QContactDetail> details;
    GeeSet *emails = folks_email_details_get_email_addresses(FOLKS_EMAIL_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(emails));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        const gchar *email = (const gchar*) folks_abstract_field_details_get_value(fd);

        QContactEmailAddress addr;
        addr.setEmailAddress(QString::fromUtf8(email));
        parseParameters(addr, fd);

        g_object_unref(fd);
        details << addr;
    }

    g_object_unref (iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPhones() const
{
    QList<QtContacts::QContactDetail> details;
    GeeSet *phones = folks_phone_details_get_phone_numbers(FOLKS_PHONE_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(phones));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        const gchar *phone = (const char*) folks_abstract_field_details_get_value(fd);

        QContactPhoneNumber number;
        number.setNumber(QString::fromUtf8(phone));
        parseParameters(number, fd);

        g_object_unref(fd);
        details << number;
    }

    g_object_unref (iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getAddresses() const
{
    QList<QtContacts::QContactDetail> details;
    GeeSet *addresses = folks_postal_address_details_get_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(addresses));

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
        parseParameters(address, fd);

        g_object_unref(fd);
        details << address;
    }

    g_object_unref (iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getIms() const
{
    QList<QtContacts::QContactDetail> details;
    GeeMultiMap *ims = folks_im_details_get_im_addresses(FOLKS_IM_DETAILS(m_individual));
    GeeSet *keys = gee_multi_map_get_keys(ims);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(keys));

    while(gee_iterator_next(iter)) {
        const gchar *key = (const gchar*) gee_iterator_get(iter);
        GeeCollection *values = gee_multi_map_get(ims, key);

        GeeIterator *iterValues = gee_iterable_iterator(GEE_ITERABLE(values));
        while(gee_iterator_next(iterValues)) {
            FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iterValues));
            const char *uri = (const char*) folks_abstract_field_details_get_value(fd);

            QContactOnlineAccount account;
            account.setAccountUri(QString::fromUtf8(uri));
            account.setProtocol(static_cast<QContactOnlineAccount::Protocol>(onlineAccountProtocolFromString(QString::fromUtf8(key))));
            parseParameters(account, fd);

            g_object_unref(fd);
            details << account;
        }
        g_object_unref (iterValues);
    }

    g_object_unref (iter);

    return details;
}

QtContacts::QContactDetail QIndividual::getTimeZone() const
{
    //TODO
    return QContactDetail();
}

QList<QtContacts::QContactDetail> QIndividual::getUrls() const
{
    QList<QtContacts::QContactDetail> details;
    GeeSet *urls = folks_url_details_get_urls(FOLKS_URL_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(urls));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        const char *url = (const char*) folks_abstract_field_details_get_value(fd);

        QContactUrl detail;
        detail.setUrl(QString::fromUtf8(url));
        parseParameters(detail, fd);

        g_object_unref(fd);
        details << detail;
    }

    g_object_unref (iter);

    return details;
}

bool QIndividual::fieldsContains(Fields fields, Field value) const
{
    return (fields.testFlag(QIndividual::All) || fields.testFlag(value));
}

QtContacts::QContact QIndividual::copy(Fields fields)
{
    QList<QContactDetail> details;
    QContact result;


    if (fields.testFlag(QIndividual::All)) {
        result = contact();
    } else {
        QContact fullContact = contact();

        // mandatory
        details << fullContact.detail<QContactGuid>();
        Q_FOREACH(QContactDetail det, fullContact.details<QContactExtendedDetail>()) {
            details << det;
        }

        if (fieldsContains(fields, QIndividual::Name)) {
            details << fullContact.detail<QContactName>();
        }


        if (fieldsContains(fields, QIndividual::FullName)) {
            details << fullContact.detail<QContactDisplayLabel>();
        }

        if (fieldsContains(fields, QIndividual::NickName)) {
            details << fullContact.detail<QContactNickname>();
        }

        if (fieldsContains(fields, QIndividual::Birthday)) {
            details << fullContact.detail<QContactBirthday>();
        }

        if (fieldsContains(fields, QIndividual::Photo)) {
            details << fullContact.detail<QContactAvatar>();
        }

        if (fieldsContains(fields, QIndividual::Role)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactOrganization>()) {
                details << det;
            }
        }

        if (fieldsContains(fields, QIndividual::Email)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactEmailAddress>()) {
                details << det;
            }
        }

        if (fieldsContains(fields, QIndividual::Phone)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactPhoneNumber>()) {
                details << det;
            }
        }

        if (fieldsContains(fields, QIndividual::Address)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactAddress>()) {
                details << det;
            }
        }

        if (fieldsContains(fields, QIndividual::Url)) {
            Q_FOREACH(QContactDetail det, fullContact.details<QContactUrl>()) {
                details << det;
            }
        }

        if (fieldsContains(fields, QIndividual::TimeZone)) {
            //TODO
        }

        Q_FOREACH(QContactDetail det, details) {
            result.appendDetail(det);
        }
    }


    return result;
}

QtContacts::QContact &QIndividual::contact()
{
    if (m_contact.isEmpty()) {
        updateContact();
    }
    return m_contact;
}

void QIndividual::updateContact()
{
    m_contact = QContact();
    m_contact.appendDetail(getUid());
    m_contact.appendDetail(getName());
    m_contact.appendDetail(getFullName());
    m_contact.appendDetail(getNickname());
    m_contact.appendDetail(getBirthday());
    m_contact.appendDetail(getPhoto());
    m_contact.appendDetail(getTimeZone());
    Q_FOREACH(QContactDetail detail, getClientPidMap()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getRoles()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getEmails()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getPhones()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getAddresses()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getIms()) {
        m_contact.appendDetail(detail);
    }
    Q_FOREACH(QContactDetail detail, getUrls()) {
        m_contact.appendDetail(detail);
    }
}

void QIndividual::updateFullName(const QtContacts::QContactDetail &detail, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QContactDetail originalLabel = m_contact.detail(QContactDetail::TypeDisplayLabel);
    if (FOLKS_IS_NAME_DETAILS(udata->m_persona) && (originalLabel != detail)) {
        const QContactDisplayLabel *label = static_cast<const QContactDisplayLabel*>(&detail);

        folks_name_details_change_full_name(FOLKS_NAME_DETAILS(udata->m_persona),
                                            label->label().toUtf8().data(),
                                            (GAsyncReadyCallback) updateDetailsDone,
                                            data);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateName(const QtContacts::QContactDetail &detail, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QContactDetail originalName = m_contact.detail(QContactDetail::TypeName);
    if (FOLKS_IS_NAME_DETAILS(udata->m_persona) && (originalName != detail)) {
        const QContactName *name = static_cast<const QContactName*>(&detail);
        FolksStructuredName *sn = folks_name_details_get_structured_name(FOLKS_NAME_DETAILS(m_individual));
        if (!sn) {
            sn = folks_structured_name_new(name->lastName().toUtf8().data(),
                                           name->firstName().toUtf8().data(),
                                           name->middleName().toUtf8().data(),
                                           name->prefix().toUtf8().data(),
                                           name->suffix().toUtf8().data());
        } else {
            folks_structured_name_set_family_name(sn, name->lastName().toUtf8().data());
            folks_structured_name_set_given_name(sn, name->firstName().toUtf8().data());
            folks_structured_name_set_additional_names(sn, name->middleName().toUtf8().data());
            folks_structured_name_set_prefixes(sn, name->prefix().toUtf8().data());
            folks_structured_name_set_suffixes(sn, name->suffix().toUtf8().data());
        }

        folks_name_details_change_structured_name(FOLKS_NAME_DETAILS(udata->m_persona),
                                                  sn,
                                                  (GAsyncReadyCallback) updateDetailsDone,
                                                  data);
        g_object_unref(sn);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateNickname(const QtContacts::QContactDetail &detail, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QContactDetail originalNickname = m_contact.detail(QContactDetail::TypeNickname);
    if (FOLKS_IS_NAME_DETAILS(udata->m_persona) && (originalNickname != detail)) {
        const QContactNickname *nickname = static_cast<const QContactNickname*>(&detail);
        folks_name_details_change_nickname(FOLKS_NAME_DETAILS(udata->m_persona),
                                           nickname->nickname().toUtf8().data(),
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           data);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateBirthday(const QtContacts::QContactDetail &detail, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QContactDetail originalBirthday = m_contact.detail(QContactDetail::TypeBirthday);
    if (FOLKS_IS_BIRTHDAY_DETAILS(udata->m_persona) && (originalBirthday != detail)) {
        const QContactBirthday *birthday = static_cast<const QContactBirthday*>(&detail);
        GDateTime *dateTime = NULL;
        if (!birthday->isEmpty()) {
            dateTime = g_date_time_new_from_unix_utc(birthday->dateTime().toMSecsSinceEpoch() / 1000);
        }
        folks_birthday_details_change_birthday(FOLKS_BIRTHDAY_DETAILS(udata->m_persona),
                                               dateTime,
                                               (GAsyncReadyCallback) updateDetailsDone,
                                               data);
        g_date_time_unref(dateTime);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updatePhoto(const QtContacts::QContactDetail &detail, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QContactDetail originalAvatar = m_contact.detail(QContactDetail::TypeAvatar);
    if (FOLKS_IS_AVATAR_DETAILS(udata->m_persona) && (detail != originalAvatar)) {
        //TODO:
        const QContactAvatar *avatar = static_cast<const QContactAvatar*>(&detail);
        folks_avatar_details_change_avatar(FOLKS_AVATAR_DETAILS(udata->m_persona),
                                           0,
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           data);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateTimezone(const QtContacts::QContactDetail &detail, void* data)
{
    //TODO
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
}

void QIndividual::updateRoles(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalOrganizations = m_contact.details(QContactDetail::TypeOrganization);

    if (FOLKS_IS_ROLE_DETAILS(udata->m_persona) && !detailListIsEqual(originalOrganizations, details)) {
        GeeSet *roleSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactOrganization *org = static_cast<const QContactOrganization*>(&detail);

            // The role values can not be NULL
            const gchar* title = org->title().toUtf8().data();
            const gchar* name = org->name().toUtf8().data();
            const gchar* roleName = org->role().toUtf8().data();

            FolksRole *role = folks_role_new(title ? title : "", name ? name : "", "");
            folks_role_set_role(role, roleName ? roleName : "");

            FolksRoleFieldDetails *fieldDetails = folks_role_field_details_new(role, NULL);
            parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(fieldDetails), detail);
            gee_collection_add(GEE_COLLECTION(roleSet), fieldDetails);

            g_object_unref(role);
            g_object_unref(fieldDetails);
        }

        folks_role_details_change_roles(FOLKS_ROLE_DETAILS(udata->m_persona),
                                        roleSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        data);
        g_object_unref(roleSet);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateEmails(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalEmails = m_contact.details(QContactDetail::TypeEmailAddress);

    if (FOLKS_IS_EMAIL_DETAILS(udata->m_persona) && !detailListIsEqual(originalEmails, details)) {
        GeeSet *emailSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactEmailAddress *email = static_cast<const QContactEmailAddress*>(&detail);

            if(!email->isEmpty()) {
                FolksEmailFieldDetails *emailDetails = folks_email_field_details_new(email->emailAddress().toUtf8().data(), NULL);
                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(emailDetails), detail);
                gee_collection_add(GEE_COLLECTION(emailSet), emailDetails);
                g_object_unref(emailDetails);
            }
        }
        folks_email_details_change_email_addresses(FOLKS_EMAIL_DETAILS(udata->m_persona),
                                                   emailSet,
                                                   (GAsyncReadyCallback) updateDetailsDone,
                                                   data);
        g_object_unref(emailSet);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updatePhones(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalPhones = m_contact.details(QContactDetail::TypePhoneNumber);

    if (FOLKS_IS_PHONE_DETAILS(udata->m_persona) && !detailListIsEqual(originalPhones, details)) {
        GeeSet *phoneSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactPhoneNumber *phone = static_cast<const QContactPhoneNumber*>(&detail);

            if(!phone->isEmpty()) {
                FolksPhoneFieldDetails *phoneDetails = folks_phone_field_details_new(phone->number().toUtf8().data(), NULL);
                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(phoneDetails), detail);
                gee_collection_add(GEE_COLLECTION(phoneSet), phoneDetails);
                g_object_unref(phoneDetails);
            }
        }
        folks_phone_details_change_phone_numbers(FOLKS_PHONE_DETAILS(udata->m_persona),
                                                 phoneSet,
                                                 (GAsyncReadyCallback) updateDetailsDone,
                                                 data);
        g_object_unref(phoneSet);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

bool QIndividual::detailListIsEqual(QList<QtContacts::QContactDetail> original, QList<QtContacts::QContactDetail> details)
{
    if (original.size() != details.size()) {
        return false;
    }

    Q_FOREACH(const QContactDetail& detail, details) {
        if (!original.contains(detail)) {
            return false;
        }
    }

    return true;
}

void QIndividual::updateAddresses(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalAddress = m_contact.details(QContactDetail::TypeAddress);

    if (FOLKS_IS_POSTAL_ADDRESS_DETAILS(udata->m_persona) && !detailListIsEqual(originalAddress, details)) {
        GeeSet *addressSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactAddress *address = static_cast<const QContactAddress*>(&detail);
            if (!address->isEmpty()) {
                FolksPostalAddress *postalAddress = folks_postal_address_new(address->postOfficeBox().toUtf8().data(),
                                                                             NULL,
                                                                             address->street().toUtf8().data(),
                                                                             address->locality().toUtf8().data(),
                                                                             address->region().toUtf8().data(),
                                                                             address->postcode().toUtf8().data(),
                                                                             address->country().toUtf8().data(),
                                                                             NULL,
                                                                             NULL);
                FolksPostalAddressFieldDetails *pafd = folks_postal_address_field_details_new(postalAddress, NULL);

                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(pafd), detail);
                gee_collection_add(GEE_COLLECTION(addressSet), pafd);

                g_object_unref(postalAddress);
                g_object_unref(pafd);
            }
        }

        folks_postal_address_details_change_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(udata->m_persona),
                                                             addressSet,
                                                             (GAsyncReadyCallback) updateDetailsDone,
                                                             data);
        g_object_unref(addressSet);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateIms(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalIms = m_contact.details(QContactDetail::TypeOnlineAccount);

    if (FOLKS_IS_IM_DETAILS(udata->m_persona) && !detailListIsEqual(originalIms, details)) {
        GeeMultiMap *imAddressHash = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactOnlineAccount *im = static_cast<const QContactOnlineAccount*>(&detail);
            if (!im->accountUri().isEmpty()) {
                FolksImFieldDetails *imfd = folks_im_field_details_new(im->accountUri().toUtf8().data(), NULL);
                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(imfd), detail);
                gee_multi_map_set(imAddressHash,
                                  onlineAccountProtocolFromEnum(im->protocol()).toUtf8().data(), imfd);
                g_object_unref(imfd);
            }
        }

        folks_im_details_change_im_addresses(FOLKS_IM_DETAILS(udata->m_persona),
                                             imAddressHash,
                                             (GAsyncReadyCallback) updateDetailsDone,
                                             data);
        g_object_unref(imAddressHash);
    } else {
        updateDetailsDone(G_OBJECT(udata->m_persona), NULL, data);
    }
}

void QIndividual::updateUrls(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalUrls = m_contact.details(QContactDetail::TypeUrl);

    if (FOLKS_IS_URL_DETAILS(udata->m_persona) && !detailListIsEqual(originalUrls, details)) {
        GeeSet *urlSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactUrl *url = static_cast<const QContactUrl*>(&detail);

            if(!url->isEmpty()) {
                FolksUrlFieldDetails *urlDetails = folks_url_field_details_new(url->url().toUtf8().data(), NULL);
                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(urlDetails), detail);
                gee_collection_add(GEE_COLLECTION(urlSet), urlDetails);
                g_object_unref(urlDetails);
            }
        }
        folks_url_details_change_urls(FOLKS_URL_DETAILS(udata->m_persona),
                                      urlSet,
                                      (GAsyncReadyCallback) updateDetailsDone,
                                      data);
        g_object_unref(urlSet);
    } else {
        updateDetailsDone(G_OBJECT(m_individual), NULL, data);
    }
}

void QIndividual::updateNotes(QList<QtContacts::QContactDetail> details, void* data)
{
    UpdateContactData *udata = static_cast<UpdateContactData*>(data);
    QList<QContactDetail> originalNotes = m_contact.details(QContactDetail::TypeNote);

    if (FOLKS_IS_NOTE_DETAILS(udata->m_persona) && !detailListIsEqual(originalNotes, details)) {
        GeeSet *noteSet = SET_AFD_NEW();
        Q_FOREACH(const QContactDetail& detail, details) {
            const QContactNote *note = static_cast<const QContactNote*>(&detail);

            if(!note->isEmpty()) {
                FolksNoteFieldDetails *noteDetails = folks_note_field_details_new(note->note().toUtf8().data(), NULL, 0);

                //TODO: set context
                gee_collection_add(GEE_COLLECTION(noteSet), noteDetails);
                g_object_unref(noteDetails);
            }
        }
        folks_note_details_change_notes(FOLKS_NOTE_DETAILS(udata->m_persona),
                                        noteSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        data);
        g_object_unref(noteSet);
    } else {
        updateDetailsDone(G_OBJECT(m_individual), NULL, data);
    }
}


void QIndividual::updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata)
{
    GError *error = 0;
    UpdateContactData *data = static_cast<UpdateContactData*>(userdata);

    switch(data->m_currentDetailType) {
    case QContactDetail::TypeAddress:
        if (result) {
            folks_postal_address_details_change_postal_addresses_finish(FOLKS_POSTAL_ADDRESS_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeAvatar;
            data->m_self->updatePhoto(data->m_newContact.detail(QContactDetail::TypeAvatar), data);
            return;
        }
        break;
    case QContactDetail::TypeAvatar:
        if (result) {
            folks_avatar_details_change_avatar_finish(FOLKS_AVATAR_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeBirthday;
            data->m_self->updateBirthday(data->m_newContact.detail(QContactDetail::TypeBirthday), data);
            return;
        }
        break;
    case QContactDetail::TypeBirthday:
        if (result) {
            folks_birthday_details_change_birthday_finish(FOLKS_BIRTHDAY_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeDisplayLabel;
            data->m_self->updateFullName(data->m_newContact.detail(QContactDetail::TypeDisplayLabel), data);
            return;
        }
        break;
    case QContactDetail::TypeDisplayLabel:
        if (result) {
            folks_name_details_change_full_name_finish(FOLKS_NAME_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeEmailAddress;
            data->m_self->updateEmails(data->m_newContact.details(QContactDetail::TypeEmailAddress), data);
            return;
        }
        break;

    case QContactDetail::TypeEmailAddress:
        if (result) {
            folks_email_details_change_email_addresses_finish(FOLKS_EMAIL_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeName;
            data->m_self->updateName(data->m_newContact.detail(QContactDetail::TypeName), data);
            return;
        }
        break;
    case QContactDetail::TypeName:
        if (result) {
            folks_name_details_change_structured_name_finish(FOLKS_NAME_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeNickname;
            data->m_self->updateNickname(data->m_newContact.detail(QContactDetail::TypeNickname), data);
            return;
        }
        break;
    case QContactDetail::TypeNickname:
        if (result) {
            folks_name_details_change_nickname_finish(FOLKS_NAME_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeNote;
            data->m_self->updateNotes(data->m_newContact.details(QContactDetail::TypeNote), data);
            return;
        }
        break;
    case QContactDetail::TypeNote:
        if (result) {
            folks_note_details_change_notes_finish(FOLKS_NOTE_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeOnlineAccount;
            data->m_self->updateIms(data->m_newContact.details(QContactDetail::TypeOnlineAccount), data);
            return;
        }
        break;
    case QContactDetail::TypeOnlineAccount:
        if (result) {
            folks_im_details_change_im_addresses_finish(FOLKS_IM_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeOrganization;
            data->m_self->updateRoles(data->m_newContact.details(QContactDetail::TypeOrganization), data);
            return;
        }
        break;
    case QContactDetail::TypeOrganization:
        if (result) {
            folks_role_details_change_roles_finish(FOLKS_ROLE_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypePhoneNumber;
            data->m_self->updatePhones(data->m_newContact.details(QContactDetail::TypePhoneNumber), data);
            return;
        }
        break;
    case QContactDetail::TypePhoneNumber:
        if (result) {
            folks_phone_details_change_phone_numbers_finish(FOLKS_PHONE_DETAILS(data->m_persona), result, &error);
        }
        if (!error) {
            data->m_currentDetailType = QContactDetail::TypeUrl;
            data->m_self->updateUrls(data->m_newContact.details(QContactDetail::TypeUrl), data);
            return;
        }
        break;
    case QContactDetail::TypeUrl:
        if (result) {
            folks_url_details_change_urls_finish(FOLKS_URL_DETAILS(data->m_persona), result, &error);
        }
    default:
        break;
    }

    QString errorMessage;
    if (error) {
        errorMessage = QString::fromUtf8(error->message);
        g_error_free(error);
    }

    qDebug() << "Update done" << errorMessage;
    data->m_doneCB(errorMessage, data->m_doneData);
    delete data;
}


bool QIndividual::update(const QtContacts::QContact &newContact, UpdateDoneCB cb, void* userData)
{
    QContact &originalContact = contact();
    if (newContact != originalContact) {

        UpdateContactData *data = new UpdateContactData;
        data->m_currentDetailType = QContactDetail::TypeAddress;
        data->m_newContact = newContact;
        data->m_self = this;
        data->m_doneCB = cb;
        data->m_doneData = userData;
        data->m_persona = primaryPersona();

        updateAddresses(newContact.details(QContactDetail::TypeAddress), data);
        return true;
    } else {
        qDebug() << "Contact is equal";
        return false;
    }
}

FolksIndividual *QIndividual::individual() const
{
    return m_individual;
}

bool QIndividual::update(const QString &vcard, UpdateDoneCB cb, void* data)
{
    QContact contact = VCardParser::vcardToContact(vcard);
    return update(contact, cb, data);
}

QStringList QIndividual::listParameters(FolksAbstractFieldDetails *details)
{
    GeeMultiMap *map = folks_abstract_field_details_get_parameters(details);
    GeeSet *keys = gee_multi_map_get_keys(map);
    GeeIterator *siter = gee_iterable_iterator(GEE_ITERABLE(keys));

    QStringList params;

    while (gee_iterator_next (siter)) {
        char *parameter = (char*) gee_iterator_get(siter);
        if (QString::fromUtf8(parameter) != "type") {
            qDebug() << "not suported field details" << parameter;
            // FIXME: check what to do with other parameters
            continue;
        }

        GeeCollection *args = folks_abstract_field_details_get_parameter_values(details, parameter);
        GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE (args));

        while (gee_iterator_next (iter)) {
            char *type = (char*) gee_iterator_get(iter);
            QString context(QString::fromUtf8(type).toLower());
            if (!params.contains(context)) {
                params << context;
            }
            g_free(type);
        }
    }
    return params;
}

QStringList QIndividual::listContext(const QtContacts::QContactDetail &detail)
{
    QStringList context = parseContext(detail);
    switch (detail.type()) {
        case QContactDetail::TypePhoneNumber:
            context << parsePhoneContext(detail);
            break;
        case QContactDetail::TypeAddress:
            context << parseAddressContext(detail);
            break;
        case QContactDetail::TypeOnlineAccount:
            context << parseOnlineAccountContext(detail);
            break;
        case QContactDetail::TypeUrl:
        case QContactDetail::TypeEmailAddress:
        case QContactDetail::TypeOrganization:
        default:
            break;
    }

    return context;
}

QStringList QIndividual::parseContext(const QtContacts::QContactDetail &detail)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactDetail::ContextHome] = "home";
        map[QContactDetail::ContextWork] = "work";
        map[QContactDetail::ContextOther] = "other";
    }

    QStringList strings;

    Q_FOREACH(int subType, detail.contexts()) {
        if (map.contains(subType)) {
            strings << map[subType];
        }
    }

    return strings;
}

void QIndividual::parseContext(FolksAbstractFieldDetails *fd, const QtContacts::QContactDetail &detail)
{
    Q_FOREACH (const QString &param, listContext(detail)) {
        folks_abstract_field_details_add_parameter(fd, "type", param.toUtf8().data());
    }
}

QStringList QIndividual::parsePhoneContext(const QtContacts::QContactDetail &detail)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactPhoneNumber::SubTypeLandline] = "landline";
        map[QContactPhoneNumber::SubTypeMobile] = "mobile";
        map[QContactPhoneNumber::SubTypeFax] = "fax";
        map[QContactPhoneNumber::SubTypePager] = "pager";
        map[QContactPhoneNumber::SubTypeVoice] = "voice";
        map[QContactPhoneNumber::SubTypeModem] = "modem";
        map[QContactPhoneNumber::SubTypeVideo] = "video";
        map[QContactPhoneNumber::SubTypeCar] = "car";
        map[QContactPhoneNumber::SubTypeBulletinBoardSystem] = "bulletinboard";
        map[QContactPhoneNumber::SubTypeMessagingCapable] = "messaging";
        map[QContactPhoneNumber::SubTypeAssistant] = "assistant";
        map[QContactPhoneNumber::SubTypeDtmfMenu] = "dtmfmenu";
    }

    QStringList strings;

    Q_FOREACH(int subType, static_cast<const QContactPhoneNumber*>(&detail)->subTypes()) {
        if (map.contains(subType)) {
            strings << map[subType];
        }
    }

    return strings;
}

QStringList QIndividual::parseAddressContext(const QtContacts::QContactDetail &detail)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactAddress::SubTypeParcel] = "parcel";
        map[QContactAddress::SubTypePostal] = "postal";
        map[QContactAddress::SubTypeDomestic] = "domestic";
        map[QContactAddress::SubTypeInternational] = "international";
    }

    QStringList strings;
    Q_FOREACH(int subType, static_cast<const QContactAddress*>(&detail)->subTypes()) {
        if (map.contains(subType)) {
            strings << map[subType];
        }
    }

    return strings;
}

QStringList QIndividual::parseOnlineAccountContext(const QtContacts::QContactDetail &im)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactOnlineAccount::SubTypeSip] = "sip";
        map[QContactOnlineAccount::SubTypeSipVoip] = "sipvoip";
        map[QContactOnlineAccount::SubTypeImpp] = "impp";
        map[QContactOnlineAccount::SubTypeVideoShare] = "videoshare";
    }

    QSet<QString> strings;
    const QContactOnlineAccount *acc = static_cast<const QContactOnlineAccount*>(&im);
    Q_FOREACH(int subType, acc->subTypes()) {
        if (map.contains(subType)) {
            strings << map[subType];
        }
    }

    // Make compatible with contact importer
    if (acc->protocol() == QContactOnlineAccount::ProtocolJabber) {
        strings << map[QContactOnlineAccount::SubTypeImpp];
    } else if (acc->protocol() == QContactOnlineAccount::ProtocolJabber) {
        strings << map[QContactOnlineAccount::SubTypeSip];
    }

    return strings.toList();
}

void QIndividual::parseParameters(QtContacts::QContactDetail &detail, FolksAbstractFieldDetails *fd)
{
    QStringList params = listParameters(fd);
    QList<int> context = contextsFromParameters(params);
    if (!context.isEmpty()) {
        detail.setContexts(context);
    }

    switch (detail.type()) {
        case QContactDetail::TypePhoneNumber:
            parsePhoneParameters(detail, params);
            break;
        case QContactDetail::TypeAddress:
            parseAddressParameters(detail, params);
            break;
        case QContactDetail::TypeOnlineAccount:
            parseOnlineAccountParameters(detail, params);
            break;
        case QContactDetail::TypeUrl:
        case QContactDetail::TypeEmailAddress:
        case QContactDetail::TypeOrganization:
        default:
            break;
    }
}

QList<int> QIndividual::contextsFromParameters(QStringList &parameters)
{
    static QMap<QString, int> map;

    // populate the map once
    if (map.isEmpty()) {
        map["home"] = QContactDetail::ContextHome;
        map["work"] = QContactDetail::ContextWork;
        map["other"] = QContactDetail::ContextOther;
    }

    QList<int> values;
    QStringList accepted;
    Q_FOREACH(const QString &param, parameters) {
        if (map.contains(param)) {
            accepted << param;
            values << map[param];
        }
    }

    Q_FOREACH(const QString &param, accepted) {
        parameters.removeOne(param);
    }

    return values;
}

void QIndividual::parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &params)
{
    // populate the map once
    static QMap<QString, int> mapTypes;
    if (mapTypes.isEmpty()) {
        mapTypes["landline"] = QContactPhoneNumber::SubTypeLandline;
        mapTypes["mobile"] = QContactPhoneNumber::SubTypeMobile;
        mapTypes["cell"] = QContactPhoneNumber::SubTypeMobile;
        mapTypes["fax"] = QContactPhoneNumber::SubTypeFax;
        mapTypes["pager"] = QContactPhoneNumber::SubTypePager;
        mapTypes["voice"] = QContactPhoneNumber::SubTypeVoice;
        mapTypes["modem"] = QContactPhoneNumber::SubTypeModem;
        mapTypes["video"] = QContactPhoneNumber::SubTypeVideo;
        mapTypes["car"] = QContactPhoneNumber::SubTypeCar;
        mapTypes["bulletinboard"] = QContactPhoneNumber::SubTypeBulletinBoardSystem;
        mapTypes["messaging"] = QContactPhoneNumber::SubTypeMessagingCapable;
        mapTypes["assistant"] = QContactPhoneNumber::SubTypeAssistant;
        mapTypes["dtmfmenu"] = QContactPhoneNumber::SubTypeDtmfMenu;
    }

    QList<int> subTypes;
    Q_FOREACH(const QString &param, params) {
        if (mapTypes.contains(param)) {
            subTypes << mapTypes[param];
        } else {
            qWarning() << "Invalid phone parameter:" << param;
        }
    }

    if (!subTypes.isEmpty()) {
        static_cast<QContactPhoneNumber*>(&phone)->setSubTypes(subTypes);
    }
}

void QIndividual::parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters)
{
    static QMap<QString, int> map;

    // populate the map once
    if (map.isEmpty()) {
        map["parcel"] = QContactAddress::SubTypeParcel;
        map["postal"] = QContactAddress::SubTypePostal;
        map["domestic"] = QContactAddress::SubTypeDomestic;
        map["international"] = QContactAddress::SubTypeInternational;
    }

    QList<int> values;

    Q_FOREACH(const QString &param, parameters) {
        if (map.contains(param)) {
            values << map[param];
        } else {
            qWarning() << "invalid Address subtype" << param;
        }
    }

    if (!values.isEmpty()) {
        static_cast<QContactAddress*>(&address)->setSubTypes(values);
    }
}

void QIndividual::parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters)
{
    static QMap<QString, int> map;

    // populate the map once
    if (map.isEmpty()) {
        map["sip"] = QContactOnlineAccount::SubTypeSip;
        map["sipvoip"] = QContactOnlineAccount::SubTypeSipVoip;
        map["impp"] = QContactOnlineAccount::SubTypeImpp;
        map["videoshare"] = QContactOnlineAccount::SubTypeVideoShare;
    }

    QSet<int> values;

    Q_FOREACH(const QString &param, parameters) {
        if (map.contains(param)) {
            values << map[param];
        } else {
            qWarning() << "invalid IM subtype" << param;
        }
    }

    // Make compatible with contact importer
    QContactOnlineAccount *acc = static_cast<QContactOnlineAccount*>(&im);
    if (acc->protocol() == QContactOnlineAccount::ProtocolJabber) {
        values << QContactOnlineAccount::SubTypeImpp;
    } else if (acc->protocol() == QContactOnlineAccount::ProtocolJabber) {
        values << QContactOnlineAccount::SubTypeSip;
    }

    if (!values.isEmpty()) {
        static_cast<QContactOnlineAccount*>(&im)->setSubTypes(values.toList());
    }
}

int QIndividual::onlineAccountProtocolFromString(const QString &protocol)
{
    static QMap<QString, QContactOnlineAccount::Protocol> map;

    // populate the map once
    if (map.isEmpty()) {
        map["aim"] = QContactOnlineAccount::ProtocolAim;
        map["icq"] = QContactOnlineAccount::ProtocolIcq;
        map["irc"] = QContactOnlineAccount::ProtocolIrc;
        map["jabber"] = QContactOnlineAccount::ProtocolJabber;
        map["msn"] = QContactOnlineAccount::ProtocolMsn;
        map["qq"] = QContactOnlineAccount::ProtocolQq;
        map["skype"] = QContactOnlineAccount::ProtocolSkype;
        map["yahoo"] = QContactOnlineAccount::ProtocolYahoo;
    }

    if (map.contains(protocol)) {
        return map[protocol];
    } else {
        qWarning() << "invalid IM protocol" << protocol;
    }

    return QContactOnlineAccount::ProtocolUnknown;
}

QString QIndividual::onlineAccountProtocolFromEnum(int protocol)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactOnlineAccount::ProtocolAim] = "aim";
        map[QContactOnlineAccount::ProtocolIcq] = "icq";
        map[QContactOnlineAccount::ProtocolIrc] = "irc";
        map[QContactOnlineAccount::ProtocolJabber] = "jabber";
        map[QContactOnlineAccount::ProtocolMsn] = "msn";
        map[QContactOnlineAccount::ProtocolQq] = "qq";
        map[QContactOnlineAccount::ProtocolSkype] = "skype";
        map[QContactOnlineAccount::ProtocolYahoo] = "yahoo";
        map[QContactOnlineAccount::ProtocolUnknown] = "unknown";
    }

    if (map.contains(protocol)) {
        return map[protocol];
    } else {
        qWarning() << "invalid IM protocol" << protocol;
    }

    return map[QContactOnlineAccount::ProtocolUnknown];
}

GHashTable *QIndividual::parseDetails(const QtContacts::QContact &contact)
{
    GHashTable *details = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) QIndividualUtils::gValueSliceFree);
     GValue *value;

     /*
      * Addresses
      */
    QList<QContactAddress> addresses = contact.details<QContactAddress>();
    if(addresses.size() > 0) {
    value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);
    Q_FOREACH(const QContactAddress& address, addresses) {
        if(!address.isEmpty()) {
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
            parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(pafd), address);
            gee_collection_add(collection, pafd);

            g_object_unref(pafd);
            g_object_unref(postalAddress);
            }
        }
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_POSTAL_ADDRESSES, value);
    }


    /*
      * Avatar
      */
     QContactAvatar avatar = contact.detail<QContactAvatar>();
     if(!avatar.isEmpty()) {
         value = QIndividualUtils::gValueSliceNew(G_TYPE_FILE_ICON);
         QUrl avatarUri = avatar.imageUrl();
         if(!avatarUri.isEmpty()) {
             QString formattedUri = avatarUri.toString(QUrl::RemoveUserInfo);
             if(!formattedUri.isEmpty()) {
                 GFile *avatarFile = g_file_new_for_uri(formattedUri.toUtf8().data());
                 GFileIcon *avatarFileIcon = G_FILE_ICON(g_file_icon_new(avatarFile));
                 g_value_take_object(value, avatarFileIcon);

                 QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_AVATAR, value);
                 g_clear_object((GObject**) &avatarFile);
             }
         }
     }

    /*
    * Birthday
    */
    QContactBirthday birthday = contact.detail<QContactBirthday>();
    if(!birthday.isEmpty()) {
        value = QIndividualUtils::gValueSliceNew(G_TYPE_DATE_TIME);
        GDateTime *dateTime = g_date_time_new_from_unix_utc(birthday.dateTime().toMSecsSinceEpoch() / 1000);
        g_value_set_boxed(value, dateTime);

        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_BIRTHDAY, value);
        g_date_time_unref(dateTime);
    }

    /*
    * Email addresses
    */
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details,
                                                FOLKS_PERSONA_DETAIL_EMAIL_ADDRESSES, value, QContactEmailAddress,
                                                FOLKS_TYPE_EMAIL_FIELD_DETAILS, emailAddress);

    /*
    * Favorite
    */
    QContactFavorite favorite = contact.detail<QContactFavorite>();
    if(!favorite.isEmpty()) {
        value = QIndividualUtils::gValueSliceNew(G_TYPE_BOOLEAN);
        g_value_set_boolean(value, favorite.isFavorite());

        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IS_FAVOURITE, value);
    }

    /*
    * Gender
    */
    QContactGender gender = contact.detail<QContactGender>();
    if(!gender.isEmpty()) {
        value = QIndividualUtils::gValueSliceNew(FOLKS_TYPE_GENDER);
        FolksGender genderEnum = FOLKS_GENDER_UNSPECIFIED;
        if(gender.gender() == QContactGender::GenderMale) {
            genderEnum = FOLKS_GENDER_MALE;
        } else if(gender.gender() == QContactGender::GenderFemale) {
            genderEnum = FOLKS_GENDER_FEMALE;
        }
        g_value_set_enum(value, genderEnum);

        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_GENDER, value);
    }

    /*
    * Names
    */
    QContactName name = contact.detail<QContactName>();
    if(!name.isEmpty()) {
        value = QIndividualUtils::gValueSliceNew(FOLKS_TYPE_STRUCTURED_NAME);
        FolksStructuredName *sn = folks_structured_name_new(name.lastName().toUtf8().data(),
                                                            name.firstName().toUtf8().data(),
                                                            name.middleName().toUtf8().data(),
                                                            name.prefix().toUtf8().data(),
                                                            name.suffix().toUtf8().data());
        g_value_take_object(value, sn);
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_STRUCTURED_NAME, value);
    }

    QContactDisplayLabel displayLabel = contact.detail<QContactDisplayLabel>();
    if(!displayLabel.label().isEmpty()) {
        value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
        g_value_set_string(value, displayLabel.label().toUtf8().data());
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_FULL_NAME, value);

        // FIXME: check if those values should all be set to the same thing
        value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
        g_value_set_string(value, displayLabel.label().toUtf8().data());
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ALIAS, value);
    }

    /*
    * Notes
    */
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details,
                                                FOLKS_PERSONA_DETAIL_NOTES, value, QContactNote,
                                                FOLKS_TYPE_NOTE_FIELD_DETAILS, note);



    /*
    * OnlineAccounts
    */
    QList<QContactOnlineAccount> accounts = contact.details<QContactOnlineAccount>();
    if(!accounts.isEmpty()) {
        QMultiMap<QString, QString> providerUidMap;

        Q_FOREACH(const QContactOnlineAccount &account, accounts) {
            if (!account.isEmpty()) {
                providerUidMap.insert(onlineAccountProtocolFromEnum(account.protocol()), account.accountUri());
            }
        }

        if(!providerUidMap.isEmpty()) {
            //TODO: add account type and subtype
            value = QIndividualUtils::asvSetStrNew(providerUidMap);
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IM_ADDRESSES, value);
        }
    }

    /*
    * Organization
    */
    QList<QContactOrganization> orgs = contact.details<QContactOrganization>();
    if(orgs.size() > 0) {
        value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);
        Q_FOREACH(const QContactOrganization& org, orgs) {
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
                parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(rfd), org);
                gee_collection_add(collection, rfd);

                g_object_unref(rfd);
                g_object_unref(role);
            }
        }
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ROLES, value);
    }

     /*
      * Phone Numbers
      */
     QList<QContactPhoneNumber> phones = contact.details<QContactPhoneNumber>();
     if (phones.size() > 0) {
         value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);
         Q_FOREACH(const QContactPhoneNumber &phone, phones) {
             if(!phone.isEmpty()) {
                 QIndividualUtils::gValueGeeSetAddStringFieldDetails(value,
                                                                     FOLKS_TYPE_PHONE_FIELD_DETAILS,
                                                                     phone.number().toUtf8().data(),
                                                                     phone);
             }
         }
         QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_PHONE_NUMBERS, value);
     }

    /*
    * URLs
    */
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details,
                                                FOLKS_PERSONA_DETAIL_URLS, value, QContactUrl,
                                                FOLKS_TYPE_URL_FIELD_DETAILS, url);
    return details;
}

void QIndividual::folksIndividualAggregatorAddPersonaFromDetailsDone(GObject *source,
                                                                     GAsyncResult *res,
                                                                     QIndividual *individual)
{
}

FolksPersona* QIndividual::primaryPersona() const
{
    if(m_individual == 0) {
        return 0;
    }

    FolksPersona *retval = NULL;
    GeeSet *personas = folks_individual_get_personas(m_individual);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    while(retval == NULL && gee_iterator_next(iter)) {
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
        FolksPersonaStore *primaryStore = folks_individual_aggregator_get_primary_store(m_aggregator);
        if(folks_persona_get_store(persona) == primaryStore) {
            retval = persona;
            g_object_ref(retval);
        }

        g_object_unref(persona);
    }
    g_object_unref (iter);

    return retval;
}

} //namespace

