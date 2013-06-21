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

class UpdateContactData
{
public:
    QContact m_newContact;
    galera::QIndividual *m_self;

    QList<QContactDetail> m_details;
    QContactDetail m_currentDetail;

    QObject *m_object;
    QMetaMethod m_slot;
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

    #define SET_PERSONA_NEW() \
        GEE_SET(gee_hash_set_new(FOLKS_TYPE_PERSONA, \
                                 (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                                 NULL, \
                                 NULL, \
                                 NULL, \
                                 NULL, \
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

    #define SET_PERSONA_NEW() \
        GEE_SET(gee_hash_set_new(FOLKS_TYPE_PERSONA, \
                             (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                             NULL, \
                             NULL))


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
        details, cDetails, key, value, q_type, g_type, member) \
{ \
    if(cDetails.size() > 0) { \
        value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT); \
        Q_FOREACH(const q_type& detail, cDetails) { \
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

    static FolksAbstractFieldDetails *getDetails(GeeSet *set, const QString &uri)
    {
        Q_ASSERT(set);

        int pos = 0;
        int size = 0;
        QStringList index = uri.split(".");
        gpointer* values = gee_collection_to_array(GEE_COLLECTION(set), &size);

        if (size == 0) {
            return 0;
        } else if (index.count() == 2) {
            pos = index[1].toInt() - 1;
            Q_ASSERT(pos >= 0);
            Q_ASSERT(pos < size);
        }
        return FOLKS_ABSTRACT_FIELD_DETAILS(values[pos]);
    }

    static FolksPersona *personaFromUri(const QString &uri, FolksIndividual *individual, FolksPersona *defaultPersona)
    {
        Q_ASSERT(individual);

        if (uri.isEmpty()) {
            //TODO
            qWarning() << "NO PERSONA";
            return defaultPersona;
        } else {

            GeeSet *personas = folks_individual_get_personas(individual);
            QStringList index = uri.split(".");
            Q_ASSERT(index.count() >= 1);
            int pos = index[0].toInt() - 1;
            int size = 0;
            gpointer* values = gee_collection_to_array(GEE_COLLECTION(personas), &size);
            Q_ASSERT(pos >= 0);
            Q_ASSERT(pos < size);

            return FOLKS_PERSONA(values[pos]);
        }
    }

}; // class

QIndividual::QIndividual(FolksIndividual *individual, FolksIndividualAggregator *aggregator)
    : m_individual(individual),
      m_primaryPersona(0),
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
    return details;
}

void QIndividual::appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QtContacts::QContactDetail detail, const QString &personaIndex, bool readOnly) const
{
    if (!detail.isEmpty()) {
        detail.setDetailUri(personaIndex);
        if (readOnly) {
            QContactManagerEngine::setDetailAccessConstraints(&detail, QContactDetail::ReadOnly);
        }
        list->append(detail);
    }
}

void QIndividual::appendDetailsForPersona(QList<QtContacts::QContactDetail> *list, QList<QtContacts::QContactDetail> details, const QString &personaIndex, bool readOnly) const
{
    int subIndex = 1;
    Q_FOREACH(QContactDetail detail, details) {
        appendDetailsForPersona(list, detail, QString("%1.%2").arg(personaIndex).arg(subIndex), readOnly);
        subIndex++;
    }
}


QList<QContactDetail> QIndividual::getDetails() const
{
    int personaIndex = 1;
    QList<QContactDetail> details;
    GeeSet *personas = folks_individual_get_personas(m_individual);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    while(gee_iterator_next(iter)) {
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));

        int wsize = 0;
        gchar **wproperties = folks_persona_get_writeable_properties(persona, &wsize);
        //"gender", "local-ids", "avatar", "postal-addresses", "urls", "phone-numbers", "structured-name",
        //"anti-links", "im-addresses", "is-favourite", "birthday", "notes", "roles", "email-addresses",
        //"web-service-addresses", "groups", "full-name"
        QStringList wPropList;
        for(int i=0; i < wsize; i++) {
            wPropList << wproperties[i];
        }

        QString index = QString::number(personaIndex);

        appendDetailsForPersona(&details, getPersonaName(persona), index, !wPropList.contains("structured-name"));
        appendDetailsForPersona(&details, getPersonaFullName(persona), index, !wPropList.contains("full-name"));
        appendDetailsForPersona(&details, getPersonaNickName(persona), index, !wPropList.contains("nickname"));
        appendDetailsForPersona(&details, getPersonaBirthday(persona), index, !wPropList.contains("birthday"));
        appendDetailsForPersona(&details, getPersonaRoles(persona), index, !wPropList.contains("roles"));
        appendDetailsForPersona(&details, getPersonaEmails(persona), index, !wPropList.contains("email-addresses"));
        appendDetailsForPersona(&details, getPersonaPhones(persona), index, !wPropList.contains("phone-numbers"));
        appendDetailsForPersona(&details, getPersonaAddresses(persona), index, !wPropList.contains("postal-addresses"));
        appendDetailsForPersona(&details, getPersonaIms(persona), index, !wPropList.contains("im-addresses"));
        appendDetailsForPersona(&details, getPersonaUrls(persona), index, !wPropList.contains("urls"));
        personaIndex++;
    }
    return details;
}

QContactDetail QIndividual::getPersonaName(FolksPersona *persona) const
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
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaFullName(FolksPersona *persona) const
{
    if (!FOLKS_IS_NAME_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactDisplayLabel detail;
    const gchar *fullName = folks_name_details_get_full_name(FOLKS_NAME_DETAILS(persona));
    if (fullName && strlen(fullName)) {
        detail.setLabel(QString::fromUtf8(fullName));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaNickName(FolksPersona *persona) const
{
    if (!FOLKS_IS_NAME_DETAILS(persona)) {
        return QContactDetail();
    }

    QContactNickname detail;
    const gchar* nickname = folks_name_details_get_nickname(FOLKS_NAME_DETAILS(persona));
    if (nickname && strlen(nickname)) {
        detail.setNickname(QString::fromUtf8(nickname));
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaBirthday(FolksPersona *persona) const
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
    }
    return detail;
}

QtContacts::QContactDetail QIndividual::getPersonaPhoto(FolksPersona *persona) const
{
    // TODO
    return QContactAvatar();
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaRoles(FolksPersona *persona) const
{
    if (!FOLKS_IS_ROLE_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *roles = folks_role_details_get_roles(FOLKS_ROLE_DETAILS(persona));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(roles));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        FolksRole *role = FOLKS_ROLE(folks_abstract_field_details_get_value(fd));

        QContactOrganization org;
        QString field;
        field = QString::fromUtf8(folks_role_get_organisation_name(role));
        if (field.isEmpty()) {
            org.setName(field);
        }
        field = QString::fromUtf8(folks_role_get_title(role));
        if (!field.isEmpty()) {
            org.setTitle(field);
        }
        parseParameters(org, fd);

        g_object_unref(fd);
        details << org;
    }

    g_object_unref (iter);

    return details;
}

QList<QtContacts::QContactDetail> QIndividual::getPersonaEmails(FolksPersona *persona) const
{
    if (!FOLKS_IS_EMAIL_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *emails = folks_email_details_get_email_addresses(FOLKS_EMAIL_DETAILS(persona));
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

QList<QtContacts::QContactDetail> QIndividual::getPersonaPhones(FolksPersona *persona) const
{
    if (!FOLKS_IS_PHONE_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *phones = folks_phone_details_get_phone_numbers(FOLKS_PHONE_DETAILS(persona));
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

QList<QtContacts::QContactDetail> QIndividual::getPersonaAddresses(FolksPersona *persona) const
{
    if (!FOLKS_IS_POSTAL_ADDRESS_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *addresses = folks_postal_address_details_get_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(persona));
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

QList<QtContacts::QContactDetail> QIndividual::getPersonaIms(FolksPersona *persona) const
{
    if (!FOLKS_IS_IM_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeMultiMap *ims = folks_im_details_get_im_addresses(FOLKS_IM_DETAILS(persona));
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

QList<QtContacts::QContactDetail> QIndividual::getPersonaUrls(FolksPersona *persona) const
{
    if (!FOLKS_IS_URL_DETAILS(persona)) {
        return QList<QtContacts::QContactDetail>();
    }

    QList<QtContacts::QContactDetail> details;
    GeeSet *urls = folks_url_details_get_urls(FOLKS_URL_DETAILS(persona));
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
    Q_FOREACH(QContactDetail detail, getClientPidMap()) {
        m_contact.appendDetail(detail);
    }

    Q_FOREACH(QContactDetail detail, getDetails()) {
        m_contact.appendDetail(detail);
    }
}

void QIndividual::createPersonaForDetail(QList<QtContacts::QContactDetail> cDetails, ParseDetailsFunc parseFunc, void *data) const
{
    GHashTable *details = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) QIndividualUtils::gValueSliceFree);

    parseFunc(details, cDetails);

    folks_individual_aggregator_add_persona_from_details(m_aggregator,
                                                         NULL, // we should pass the m_individual here but due a bug on folks lest do a work around
                                                         folks_individual_aggregator_get_primary_store(m_aggregator),
                                                         details,
                                                         (GAsyncReadyCallback) createPersonaDone,
                                                         (void*) data);

    g_hash_table_destroy(details);
}

void QIndividual::updateFullName(const QtContacts::QContactDetail &detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalLabel = detailFromUri(QContactDetail::TypeDisplayLabel, detail.detailUri());

    if (persona == 0) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseFullNameDetails, data);
    } else if (FOLKS_IS_NAME_DETAILS(persona) && (originalLabel != detail)) {
        qDebug() << "Full Name diff";
        qDebug() << "\t" << originalLabel << "\n\t" << detail;

        const QContactDisplayLabel *label = static_cast<const QContactDisplayLabel*>(&detail);

        folks_name_details_change_full_name(FOLKS_NAME_DETAILS(persona),
                                            label->label().toUtf8().data(),
                                            (GAsyncReadyCallback) updateDetailsDone,
                                            data);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateName(const QtContacts::QContactDetail &detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalName = detailFromUri(QContactDetail::TypeName, detail.detailUri());

    if (persona == 0) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseNameDetails, data);
    } else if (FOLKS_IS_NAME_DETAILS(persona) && (originalName != detail)) {
        qDebug() << "Name diff";
        qDebug() << "\t" << originalName << "\n\t" << detail;

        const QContactName *name = static_cast<const QContactName*>(&detail);
        FolksStructuredName *sn;
        sn = folks_structured_name_new(name->lastName().toUtf8().data(),
                                       name->firstName().toUtf8().data(),
                                       name->middleName().toUtf8().data(),
                                       name->prefix().toUtf8().data(),
                                       name->suffix().toUtf8().data());

        folks_name_details_change_structured_name(FOLKS_NAME_DETAILS(persona),
                                                  sn,
                                                  (GAsyncReadyCallback) updateDetailsDone,
                                                  data);

        g_object_unref(sn);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateNickname(const QtContacts::QContactDetail &detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalNickname = detailFromUri(QContactDetail::TypeNickname, detail.detailUri());

    if (persona == 0) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseNicknameDetails, data);
    } else if (FOLKS_IS_NAME_DETAILS(persona) && (originalNickname != detail)) {
        const QContactNickname *nickname = static_cast<const QContactNickname*>(&detail);
        folks_name_details_change_nickname(FOLKS_NAME_DETAILS(persona),
                                           nickname->nickname().toUtf8().data(),
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           data);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateBirthday(const QtContacts::QContactDetail &detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalBirthday = detailFromUri(QContactDetail::TypeBirthday, detail.detailUri());

    if (persona == 0) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseBirthdayDetails, data);
    } else if (FOLKS_IS_BIRTHDAY_DETAILS(persona) && (originalBirthday != detail)) {
        const QContactBirthday *birthday = static_cast<const QContactBirthday*>(&detail);

        GDateTime *dateTime = NULL;
        if (!birthday->isEmpty()) {
            dateTime = g_date_time_new_from_unix_utc(birthday->dateTime().toMSecsSinceEpoch() / 1000);
        }
        folks_birthday_details_change_birthday(FOLKS_BIRTHDAY_DETAILS(persona),
                                               dateTime,
                                               (GAsyncReadyCallback) updateDetailsDone,
                                               data);
        g_date_time_unref(dateTime);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updatePhoto(const QtContacts::QContactDetail &detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalAvatar = detailFromUri(QContactDetail::TypeAvatar, detail.detailUri());

    if (persona == 0) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parsePhotoDetails, data);
    } else if (FOLKS_IS_AVATAR_DETAILS(persona) && (detail != originalAvatar)) {
        //TODO:
        //const QContactAvatar *avatar = static_cast<const QContactAvatar*>(&detail);
        folks_avatar_details_change_avatar(FOLKS_AVATAR_DETAILS(persona),
                                           0,
                                           (GAsyncReadyCallback) updateDetailsDone,
                                           data);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateRole(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalRole = detailFromUri(QContactDetail::TypeOrganization, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseOrganizationDetails, data);
    } else if (FOLKS_IS_ROLE_DETAILS(persona) && (originalRole != detail)) {
        qDebug() << "Role diff";
        qDebug() << "\t" << originalRole << "\n\t" << detail;

        FolksRoleFieldDetails *roleDetails = 0;
        const QContactOrganization *cRole = static_cast<const QContactOrganization*>(&detail);
        GeeSet *roleSet = folks_role_details_get_roles(FOLKS_ROLE_DETAILS(persona));

        if (!roleSet || (gee_collection_get_size(GEE_COLLECTION(roleSet)) == 0)) {
            roleSet = SET_AFD_NEW();
        } else {
            roleDetails = FOLKS_ROLE_FIELD_DETAILS(QIndividualUtils::getDetails(roleSet, detail.detailUri()));
            // this will be unref at the end of the function
            g_object_ref(roleSet);
            g_object_ref(roleDetails);
        }

        const gchar* title = cRole->title().isEmpty() ? "" : cRole->title().toUtf8().data();
        const gchar* name = cRole->name().isEmpty() ? "" : cRole->name().toUtf8().data();
        const gchar* roleName = cRole->role().isEmpty() ? "" : cRole->role().toUtf8().data();

        FolksRole *roleValue;
        if (!roleDetails) {
            roleValue = folks_role_new(title, name, "");
            folks_role_set_role(roleValue, roleName);

            roleDetails = folks_role_field_details_new(roleValue, NULL);
            gee_collection_add(GEE_COLLECTION(roleSet), roleDetails);
        } else {
            roleValue = FOLKS_ROLE(folks_abstract_field_details_get_value(FOLKS_ABSTRACT_FIELD_DETAILS(roleDetails)));
            folks_role_set_organisation_name(roleValue, name);
            folks_role_set_title(roleValue, title);
            folks_role_set_role(roleValue, roleName);
        }

        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(roleDetails), detail);
        folks_role_details_change_roles(FOLKS_ROLE_DETAILS(persona),
                                        roleSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        data);

        g_object_unref(roleDetails);
        g_object_unref(roleSet);
        g_object_unref(roleValue);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateEmail(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalEmail = detailFromUri(QContactDetail::TypeEmailAddress, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseEmailDetails, data);
    } else if (FOLKS_IS_EMAIL_DETAILS(persona) && (originalEmail != detail)) {
        qDebug() << "email diff";
        qDebug() << "\t" << originalEmail << "\n\t" << detail;

        FolksEmailFieldDetails *emailDetails = 0;
        const QContactEmailAddress *email = static_cast<const QContactEmailAddress*>(&detail);
        GeeSet *emailSet = folks_email_details_get_email_addresses(FOLKS_EMAIL_DETAILS(persona));
        if (!emailSet || (gee_collection_get_size(GEE_COLLECTION(emailSet)) == 0)) {
            emailSet = SET_AFD_NEW();
        } else {
            emailDetails = FOLKS_EMAIL_FIELD_DETAILS(QIndividualUtils::getDetails(emailSet, detail.detailUri()));
            // this will be unref at the end of the function
            g_object_ref(emailSet);
            g_object_ref(emailDetails);
        }

        if (!emailDetails) {
            emailDetails = folks_email_field_details_new(email->emailAddress().toUtf8().data(), NULL);
            gee_collection_add(GEE_COLLECTION(emailSet), emailDetails);
        } else {
            folks_abstract_field_details_set_value(FOLKS_ABSTRACT_FIELD_DETAILS(emailDetails),
                                                   email->emailAddress().toUtf8().data());
        }

        folks_email_details_change_email_addresses(FOLKS_EMAIL_DETAILS(persona),
                                                   emailSet,
                                                   (GAsyncReadyCallback) updateDetailsDone,
                                                   data);

        g_object_unref(emailSet);
        g_object_unref(emailDetails);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updatePhone(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalPhone = detailFromUri(QContactDetail::TypePhoneNumber, detail.detailUri());
    // if we do not have a persona for this detail we need to create one
    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parsePhoneNumbersDetails, data);
    } else if (FOLKS_IS_PHONE_DETAILS(persona) && (originalPhone != detail)) {
        qDebug() << "Phone diff";
        qDebug() << "\t" << originalPhone << "\n\t" << detail;

        /// Only update the details on the current persona
        FolksPhoneFieldDetails *phoneDetails = 0;
        const QContactPhoneNumber *phone = static_cast<const QContactPhoneNumber*>(&detail);
        GeeSet *phoneSet = folks_phone_details_get_phone_numbers(FOLKS_PHONE_DETAILS(persona));

        if (!phoneSet || (gee_collection_get_size(GEE_COLLECTION(phoneSet)) == 0)) {
            phoneSet = SET_AFD_NEW();
        } else {
            phoneDetails = FOLKS_PHONE_FIELD_DETAILS(QIndividualUtils::getDetails(phoneSet, detail.detailUri()));
            // this will be unref at the end of the function
            g_object_ref(phoneSet);
            g_object_ref(phoneDetails);
        }

        if (!phoneDetails) {
            phoneDetails = folks_phone_field_details_new(phone->number().toUtf8().data(), NULL);
            gee_collection_add(GEE_COLLECTION(phoneSet), phoneDetails);
        } else {
            folks_abstract_field_details_set_value(FOLKS_ABSTRACT_FIELD_DETAILS(phoneDetails),
                                                   phone->number().toUtf8().data());
        }

        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(phoneDetails), detail);
        folks_phone_details_change_phone_numbers(FOLKS_PHONE_DETAILS(persona),
                                                 phoneSet,
                                                 (GAsyncReadyCallback) updateDetailsDone,
                                                 data);
        g_object_unref(phoneDetails);
        g_object_unref(phoneSet);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateAddress(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalAddress = detailFromUri(QContactDetail::TypeAddress, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseAddressDetails, data);
    } else if (FOLKS_IS_POSTAL_ADDRESS_DETAILS(persona) && (originalAddress != detail)) {


        FolksPostalAddressFieldDetails *addrDetails = 0;
        const QContactAddress *addr = static_cast<const QContactAddress*>(&detail);
        const QContactAddress *addro = static_cast<const QContactAddress*>(&originalAddress);

        qDebug() << "Adderess diff";
        qDebug() << "\t" << *addro <<  "subtypes:" << addro->subTypes() << "context" << addro->contexts();
        qDebug() << "\t" << *addr <<  "subtypes:" << addr->subTypes() << "context" << addr->contexts();


        qDebug() << "SubTypes:" << addr->subTypes();
        GeeSet *addrSet = folks_postal_address_details_get_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(persona));

        if (!addrSet || (gee_collection_get_size(GEE_COLLECTION(addrSet)) == 0)) {
            addrSet = SET_AFD_NEW();
        } else {
            addrDetails = FOLKS_POSTAL_ADDRESS_FIELD_DETAILS(QIndividualUtils::getDetails(addrSet, detail.detailUri()));
            // this will be unref at the end of the function
            g_object_ref(addrSet);
            g_object_ref(addrDetails);
        }

        FolksPostalAddress *addrValue;
        if (!addrDetails) {
            addrValue = folks_postal_address_new(addr->postOfficeBox().toUtf8().data(),
                                                   NULL,
                                                   addr->street().toUtf8().data(),
                                                   addr->locality().toUtf8().data(),
                                                   addr->region().toUtf8().data(),
                                                   addr->postcode().toUtf8().data(),
                                                   addr->country().toUtf8().data(),
                                                   NULL,
                                                   NULL);
            addrDetails = folks_postal_address_field_details_new(addrValue, NULL);
            gee_collection_add(GEE_COLLECTION(addrSet), addrDetails);
        } else {
            addrValue = FOLKS_POSTAL_ADDRESS(folks_abstract_field_details_get_value(FOLKS_ABSTRACT_FIELD_DETAILS(addrDetails)));
            Q_ASSERT(addrValue);
            folks_postal_address_set_po_box(addrValue, addr->postOfficeBox().toUtf8().data());
            folks_postal_address_set_street(addrValue, addr->street().toUtf8().data());
            folks_postal_address_set_locality(addrValue, addr->locality().toUtf8().data());
            folks_postal_address_set_region(addrValue, addr->region().toUtf8().data());
            folks_postal_address_set_postal_code(addrValue, addr->postcode().toUtf8().data());
            folks_postal_address_set_country(addrValue, addr->country().toUtf8().data());
        }

        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(addrDetails), detail);
        folks_postal_address_details_change_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(persona),
                                                             addrSet,
                                                             (GAsyncReadyCallback) updateDetailsDone,
                                                             data);
        g_object_unref(addrDetails);
        g_object_unref(addrSet);
        g_object_unref(addrValue);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateIm(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalIm = detailFromUri(QContactDetail::TypeOnlineAccount, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseImDetails, data);
    } else if (FOLKS_IS_IM_DETAILS(persona) && (originalIm != detail)) {
        qDebug() << "Im diff";
        qDebug() << "\t" << originalIm << "\n\t" << detail;

        const QContactOnlineAccount *im = static_cast<const QContactOnlineAccount*>(&detail);
        GeeMultiMap *imSet = folks_im_details_get_im_addresses(FOLKS_IM_DETAILS(persona));

        if (!imSet || (gee_multi_map_get_size(GEE_MULTI_MAP(imSet)) == 0)) {
            imSet = GEE_MULTI_MAP_AFD_NEW(FOLKS_TYPE_IM_FIELD_DETAILS);
        } else {
            // We can not relay on the index inside of detailUri because online account are stored in a hash
            QContactOnlineAccount oldIm = static_cast<QContactOnlineAccount>(originalIm);
            QString oldProtocolName = onlineAccountProtocolFromEnum(oldIm.protocol());
            GeeCollection *value = gee_multi_map_get(imSet, oldProtocolName.toUtf8().data());

            // Remove the old one
            if (value) {
                // if there is more than one address only remove the correct one
                if (gee_collection_get_size(value) > 1) {
                    gee_collection_remove(value, oldIm.accountUri().toUtf8().data());
                } else {
                    // otherwise remove the key
                    gee_multi_map_remove_all(imSet, oldProtocolName.toUtf8().data());
                }
            }

            g_object_ref(imSet);
        }

        // Append the new one
        FolksImFieldDetails *imDetails = folks_im_field_details_new(im->accountUri().toUtf8().data(), NULL);
        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(imDetails), detail);

        gee_multi_map_set(imSet,
                          onlineAccountProtocolFromEnum(im->protocol()).toUtf8().data(),
                          imDetails);

        folks_im_details_change_im_addresses(FOLKS_IM_DETAILS(persona),
                                             imSet,
                                             (GAsyncReadyCallback) updateDetailsDone,
                                             data);
        g_object_unref(imSet);
        g_object_unref(imDetails);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateUrl(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalUrl = detailFromUri(QContactDetail::TypeUrl, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseUrlDetails, data);
    } else if (FOLKS_IS_URL_DETAILS(persona) && (originalUrl != detail)) {
        qDebug() << "Url diff";
        qDebug() << "\t" << originalUrl << "\n\t" << detail;

        FolksUrlFieldDetails *urlDetails = 0;
        const QContactUrl *url = static_cast<const QContactUrl*>(&detail);
        GeeSet *urlSet = folks_url_details_get_urls(FOLKS_URL_DETAILS(persona));

        if (!urlSet || (gee_collection_get_size(GEE_COLLECTION(urlSet)) == 0)) {
            urlSet = SET_AFD_NEW();
        } else {
            urlDetails = FOLKS_URL_FIELD_DETAILS(QIndividualUtils::getDetails(urlSet, detail.detailUri()));

            // this will be unref at the end of the function
            g_object_ref(urlSet);
            g_object_ref(urlDetails);
        }

        if (!urlDetails) {
            urlDetails = folks_url_field_details_new(url->url().toUtf8().data(), NULL);
            gee_collection_add(GEE_COLLECTION(urlSet), urlDetails);
        } else {
            folks_abstract_field_details_set_value(FOLKS_ABSTRACT_FIELD_DETAILS(urlDetails),
                                                   url->url().toUtf8().data());
        }

        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(urlDetails), detail);
        folks_url_details_change_urls(FOLKS_URL_DETAILS(persona),
                                      urlSet,
                                      (GAsyncReadyCallback) updateDetailsDone,
                                      data);
        g_object_unref(urlDetails);
        g_object_unref(urlSet);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

void QIndividual::updateNote(QtContacts::QContactDetail detail, void* data)
{
    FolksPersona *persona = QIndividualUtils::personaFromUri(detail.detailUri(), m_individual, primaryPersona());
    QContactDetail originalNote = detailFromUri(QContactDetail::TypeNote, detail.detailUri());

    if (!persona) {
        createPersonaForDetail(QList<QContactDetail>() << detail, QIndividual::parseNoteDetails, data);
    } else if (FOLKS_IS_URL_DETAILS(persona) && (originalNote != detail)) {
        qDebug() << "Note diff";
        qDebug() << "\t" << originalNote << "\n\t" << detail;

        FolksNoteFieldDetails *noteDetails = 0;
        const QContactNote *note = static_cast<const QContactNote*>(&detail);
        GeeSet *noteSet = folks_note_details_get_notes(FOLKS_NOTE_DETAILS(persona));

        if (!noteSet || (gee_collection_get_size(GEE_COLLECTION(noteSet)) == 0)) {
            noteSet = SET_AFD_NEW();
        } else {
            noteDetails = FOLKS_NOTE_FIELD_DETAILS(QIndividualUtils::getDetails(noteSet, detail.detailUri()));

            // this will be unref at the end of the function
            g_object_ref(noteSet);
            g_object_ref(noteDetails);
        }

        if (!noteDetails) {
            noteDetails = folks_note_field_details_new(note->note().toUtf8().data(), NULL, 0);
            gee_collection_add(GEE_COLLECTION(noteSet), noteDetails);
        } else {
            folks_abstract_field_details_set_value(FOLKS_ABSTRACT_FIELD_DETAILS(noteDetails),
                                                   note->note().toUtf8().data());
        }

        parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(noteDetails), detail);
        folks_note_details_change_notes(FOLKS_NOTE_DETAILS(persona),
                                        noteSet,
                                        (GAsyncReadyCallback) updateDetailsDone,
                                        data);;
        g_object_unref(noteDetails);
        g_object_unref(noteSet);
    } else {
        updateDetailsDone(G_OBJECT(persona), NULL, data);
    }
}

QString QIndividual::callDetailChangeFinish(QtContacts::QContactDetail::DetailType detailType,
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

void QIndividual::updateDetailsSendReply(gpointer userdata, const QString &errorMessage)
{
    UpdateContactData *data = static_cast<UpdateContactData*>(userdata);
    data->m_slot.invoke(data->m_object,
                        Q_ARG(QIndividual*, data->m_self), Q_ARG(QString, errorMessage));
    delete data;
}

void QIndividual::updateDetailsSendReply(gpointer userdata, GError *error)
{
    QString errorMessage;
    if (error) {
        errorMessage = QString::fromUtf8(error->message);
        qWarning() << error->message;
        g_error_free(error);
    }
    updateDetailsSendReply(userdata, errorMessage);
}


void QIndividual::createPersonaDone(GObject *aggregator, GAsyncResult *result, gpointer userdata)
{
    qDebug() << Q_FUNC_INFO;
    UpdateContactData *data = static_cast<UpdateContactData*>(userdata);

    // A new persona was create to store the new data
    GError *error = 0;
    FolksPersona *newPersona = folks_individual_aggregator_add_persona_from_details_finish(FOLKS_INDIVIDUAL_AGGREGATOR(aggregator),
                                                                                           result,
                                                                                           &error);
    if (error) {
        updateDetailsSendReply(data, error);
    } else {
        // Link the new personas
        GeeSet *personas = folks_individual_get_personas(data->m_self->m_individual);
        GeeSet *newPersonas = SET_PERSONA_NEW();
        gee_collection_add_all(GEE_COLLECTION(newPersonas), GEE_COLLECTION(personas));
        gee_collection_add(GEE_COLLECTION(newPersonas), newPersona);
        data->m_self->m_primaryPersona = newPersona;
        folks_individual_aggregator_link_personas(data->m_self->m_aggregator, newPersonas, updateDetailsDone, userdata);
    }
}

void QIndividual::updateDetailsDone(GObject *detail, GAsyncResult *result, gpointer userdata)
{
    QString errorMessage;
    UpdateContactData *data = static_cast<UpdateContactData*>(userdata);
    if (result && !data->m_currentDetail.isEmpty()) {
        if (FOLKS_IS_PERSONA(detail)) {
            // This is a normal field update
            errorMessage = QIndividual::callDetailChangeFinish(data->m_currentDetail.type(),
                                                               FOLKS_PERSONA(detail),
                                                               result);
        } else if (FOLKS_IS_INDIVIDUAL_AGGREGATOR(detail)) {
            GError *error = 0;
            folks_individual_aggregator_link_personas_finish(FOLKS_INDIVIDUAL_AGGREGATOR(detail), result, &error);
            if (error) {
                errorMessage = QString::fromUtf8(error->message);
            }
        }

        if (!errorMessage.isEmpty()) {
            updateDetailsSendReply(data, errorMessage);
            return;
        }
    }

    if (data->m_details.isEmpty()) {
        updateDetailsSendReply(data, 0);
        return;
    }


    data->m_currentDetail = data->m_details.takeFirst();
    switch(data->m_currentDetail.type()) {
    case QContactDetail::TypeAddress:
        data->m_self->updateAddress(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeAvatar:
        data->m_self->updatePhoto(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeBirthday:
        data->m_self->updateBirthday(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeDisplayLabel:
        data->m_self->updateFullName(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeEmailAddress:
        data->m_self->updateEmail(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeName:
        data->m_self->updateName(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeNickname:
        data->m_self->updateNickname(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeNote:
        data->m_self->updateNote(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeOnlineAccount:
        data->m_self->updateIm(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeOrganization:
        data->m_self->updateRole(data->m_currentDetail, data);
        break;
    case QContactDetail::TypePhoneNumber:
        data->m_self->updatePhone(data->m_currentDetail, data);
        break;
    case QContactDetail::TypeUrl:
        data->m_self->updateUrl(data->m_currentDetail, data);
        break;
    default:
        qWarning() << "Update not implemented for" << data->m_currentDetail.type();
        updateDetailsDone(0, 0, data);
        break;
    }
}

bool QIndividual::update(const QtContacts::QContact &newContact, QObject *object, const QString &slot)
{
    int slotIndex = object->metaObject()->indexOfSlot(QMetaObject::normalizedSignature(slot.toUtf8().data()));
    if (slotIndex == -1) {
        qWarning() << "Invalid slot:" << slot << "for object" << object;
        return false;
    }

    QContact &originalContact = contact();
    if (newContact != originalContact) {

        UpdateContactData *data = new UpdateContactData;
        data->m_details = newContact.details();
        data->m_newContact = newContact;
        data->m_self = this;
        data->m_object = object;
        data->m_slot = object->metaObject()->method(slotIndex);
        updateDetailsDone(0, 0, data);
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
        m_contact = QContact();
        updateContact();
    }
}

bool QIndividual::update(const QString &vcard, QObject *object, const QString &slot)
{
    QContact contact = VCardParser::vcardToContact(vcard);
    return update(contact, object, slot);
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

    qDebug() << "PArse paramater:" << parameters;

    // populate the map once
    if (map.isEmpty()) {
        map["home"] = QContactDetail::ContextHome;
        map["work"] = QContactDetail::ContextWork;
        map["other"] = QContactDetail::ContextOther;
    }

    QList<int> values;
    QStringList accepted;
    Q_FOREACH(const QString &param, parameters) {
        if (map.contains(param.toLower())) {
            accepted << param;
            values << map[param.toLower()];
        }
    }

    Q_FOREACH(const QString &param, accepted) {
        parameters.removeOne(param);
    }

    qDebug() << "PArseed paramater (DONE):" << parameters;

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
        if (mapTypes.contains(param.toLower())) {
            subTypes << mapTypes[param.toLower()];
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
        if (map.contains(param.toLower())) {
            values << map[param.toLower()];
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
        if (map.contains(param.toLower())) {
            values << map[param.toLower()];
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

    if (map.contains(protocol.toLower())) {
        return map[protocol.toLower()];
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

GHashTable *QIndividual::parseAddressDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);

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
            parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(pafd), address);
            gee_collection_add(collection, pafd);

            g_object_unref(pafd);
            g_object_unref(postalAddress);
        }
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_POSTAL_ADDRESSES, value);
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
            GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_FILE_ICON);
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
            GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_DATE_TIME);
            GDateTime *dateTime = g_date_time_new_from_unix_utc(birthday.dateTime().toMSecsSinceEpoch() / 1000);
            g_value_set_boxed(value, dateTime);

            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_BIRTHDAY, value);
            g_date_time_unref(dateTime);
        }
    }

    return details;
}

GHashTable *QIndividual::parseEmailDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_EMAIL_ADDRESSES, value, QContactEmailAddress,
                                                FOLKS_TYPE_EMAIL_FIELD_DETAILS, emailAddress);
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
            GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_BOOLEAN);
            g_value_set_boolean(value, favorite.isFavorite());

            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IS_FAVOURITE, value);
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
            GValue *value = QIndividualUtils::gValueSliceNew(FOLKS_TYPE_GENDER);
            FolksGender genderEnum = FOLKS_GENDER_UNSPECIFIED;
            if(gender.gender() == QContactGender::GenderMale) {
                genderEnum = FOLKS_GENDER_MALE;
            } else if(gender.gender() == QContactGender::GenderFemale) {
                genderEnum = FOLKS_GENDER_FEMALE;
            }
            g_value_set_enum(value, genderEnum);

            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_GENDER, value);
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
            GValue *value = QIndividualUtils::gValueSliceNew(FOLKS_TYPE_STRUCTURED_NAME);
            FolksStructuredName *sn = folks_structured_name_new(name.lastName().toUtf8().data(),
                                                                name.firstName().toUtf8().data(),
                                                                name.middleName().toUtf8().data(),
                                                                name.prefix().toUtf8().data(),
                                                                name.suffix().toUtf8().data());
            g_value_take_object(value, sn);
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_STRUCTURED_NAME, value);
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
            GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, displayLabel.label().toUtf8().data());
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_FULL_NAME, value);

            // FIXME: check if those values should all be set to the same thing
            value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, displayLabel.label().toUtf8().data());
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ALIAS, value);
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
            GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, nickname.nickname().toUtf8().data());
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_NICKNAME, value);

            // FIXME: check if those values should all be set to the same thing
            value = QIndividualUtils::gValueSliceNew(G_TYPE_STRING);
            g_value_set_string(value, nickname.nickname().toUtf8().data());
            QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ALIAS, value);
        }
    }

    return details;
}

GHashTable *QIndividual::parseNoteDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_NOTES, value, QContactNote,
                                                FOLKS_TYPE_NOTE_FIELD_DETAILS, note);

    return details;
}

GHashTable *QIndividual::parseImDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    QMultiMap<QString, QString> providerUidMap;
    Q_FOREACH(const QContactDetail &detail, cDetails) {
        QContactOnlineAccount account = static_cast<QContactOnlineAccount>(detail);
        if (!account.isEmpty()) {
            providerUidMap.insert(onlineAccountProtocolFromEnum(account.protocol()), account.accountUri());
        }
    }

    if(!providerUidMap.isEmpty()) {
        //TODO: add account type and subtype
        GValue *value = QIndividualUtils::asvSetStrNew(providerUidMap);
        QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_IM_ADDRESSES, value);
    }

    return details;
}

GHashTable *QIndividual::parseOrganizationDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);
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
            parseContext(FOLKS_ABSTRACT_FIELD_DETAILS(rfd), org);
            gee_collection_add(collection, rfd);

            g_object_unref(rfd);
            g_object_unref(role);
        }
    }
    QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_ROLES, value);

    return details;
}

GHashTable *QIndividual::parsePhoneNumbersDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value = QIndividualUtils::gValueSliceNew(G_TYPE_OBJECT);
    Q_FOREACH(const QContactDetail &detail, cDetails) {
        QContactPhoneNumber phone = static_cast<QContactPhoneNumber>(detail);
        if(!phone.isEmpty()) {
            QIndividualUtils::gValueGeeSetAddStringFieldDetails(value,
                                                                FOLKS_TYPE_PHONE_FIELD_DETAILS,
                                                                phone.number().toUtf8().data(),
                                                                phone);
        }
    }
    QIndividualUtils::personaDetailsInsert(details, FOLKS_PERSONA_DETAIL_PHONE_NUMBERS, value);

    return details;
}

GHashTable *QIndividual::parseUrlDetails(GHashTable *details, const QList<QtContacts::QContactDetail> &cDetails)
{
    if(cDetails.size() == 0) {
        return details;
    }

    GValue *value;
    PERSONA_DETAILS_INSERT_STRING_FIELD_DETAILS(details, cDetails,
                                                FOLKS_PERSONA_DETAIL_URLS, value, QContactUrl,
                                                FOLKS_TYPE_URL_FIELD_DETAILS, url);

    return details;
}


GHashTable *QIndividual::parseDetails(const QtContacts::QContact &contact)
{
    GHashTable *details = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) QIndividualUtils::gValueSliceFree);

    parseAddressDetails(details, contact.details(QContactAddress::Type));
    parsePhotoDetails(details, contact.details(QContactAvatar::Type));
    parseBirthdayDetails(details, contact.details(QContactBirthday::Type));
    parseEmailDetails(details, contact.details(QContactEmailAddress::Type));
    parseFavoriteDetails(details, contact.details(QContactFavorite::Type));
    parseGenderDetails(details, contact.details(QContactGender::Type));
    parseNameDetails(details, contact.details(QContactName::Type));
    parseFullNameDetails(details, contact.details(QContactDisplayLabel::Type));
    parseNicknameDetails(details, contact.details(QContactNickname::Type));
    parseNoteDetails(details, contact.details(QContactNote::Type));
    parseImDetails(details, contact.details(QContactOnlineAccount::Type));
    parseOrganizationDetails(details, contact.details(QContactOrganization::Type));
    parsePhoneNumbersDetails(details, contact.details(QContactPhoneNumber::Type));
    parseUrlDetails(details, contact.details(QContactUrl::Type));

    return details;
}

void QIndividual::folksIndividualAggregatorAddPersonaFromDetailsDone(GObject *source,
                                                                     GAsyncResult *res,
                                                                     QIndividual *individual)
{
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
    g_object_unref (iter);

    return m_primaryPersona;
}

QtContacts::QContactDetail QIndividual::detailFromUri(QtContacts::QContactDetail::DetailType type, const QString &uri) const
{
    Q_FOREACH(QContactDetail detail, m_contact.details(type)) {
        if (detail.detailUri() == uri) {
            return detail;
        }
    }
    return m_contact.detail(type);
}

} //namespace

