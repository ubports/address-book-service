#include "qindividual.h"

#include "vcard/vcard-parser.h"

#include <QtVersit/QVersitDocument>
#include <QtVersit/QVersitProperty>
#include <QtVersit/QVersitWriter>
#include <QtVersit/QVersitReader>
#include <QtVersit/QVersitContactImporter>
#include <QtVersit/QVersitContactExporter>

#include <QtContacts/QContactName>
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

namespace galera
{

#define SET_AFD_NEW() \
        GEE_SET(gee_hash_set_new( \
                    FOLKS_TYPE_ABSTRACT_FIELD_DETAILS, \
                    (GBoxedCopyFunc) g_object_ref, g_object_unref, \
                    (GHashFunc) folks_abstract_field_details_hash, \
                    (GEqualFunc) folks_abstract_field_details_equal))

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
        GeeMultiMap *hashSet = GEE_MULTI_MAP(
                gee_hash_multi_map_new(G_TYPE_STRING, (GBoxedCopyFunc) g_strdup,
                        g_free,
                        FOLKS_TYPE_IM_FIELD_DETAILS,
                        g_object_ref, g_object_unref,
                        g_str_hash, g_str_equal,
                        (GHashFunc) folks_abstract_field_details_hash,
                        (GEqualFunc) folks_abstract_field_details_equal));

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

QIndividual::QIndividual(FolksIndividual *individual)
    : m_individual(individual)
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
        address.setCountry(QString::fromUtf8(folks_postal_address_get_country(addr)));
        address.setLocality(QString::fromUtf8(folks_postal_address_get_locality(addr)));
        address.setPostOfficeBox(QString::fromUtf8(folks_postal_address_get_po_box(addr)));
        address.setPostcode(QString::fromUtf8(folks_postal_address_get_postal_code(addr)));
        address.setRegion(QString::fromUtf8(folks_postal_address_get_region(addr)));
        address.setStreet(QString::fromUtf8(folks_postal_address_get_street(addr)));
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
            //TODO
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

bool QIndividual::update(const QtContacts::QContact &contact)
{
    if (contact != this->contact()) {
        qDebug() << "Contact needs update";

        QList<QContactDetail> originalDetails = this->contact().details();
        QList<QContactDetail> newDetails = contact.details();
        qDebug() << "size of details" << originalDetails.size() << newDetails.size();
        for(int i=0; i < originalDetails.size(); i++) {
            //qDebug() << "\t" << originalDetails[i];
            if (!originalDetails.contains(newDetails[i])) {
                qDebug() << "orig details:" << originalDetails[i] << "/"  << originalDetails[i].contexts();
                qDebug() << "new details:" << newDetails[i] << "/" << newDetails[i].contexts();
                qDebug() << "-------------------------------------";

                if (originalDetails[i].type() == QContactDetail::TypeOnlineAccount) {
                    QContactOnlineAccount *acc = static_cast<QContactOnlineAccount*>(&originalDetails[i]);
                    QContactOnlineAccount *acc2 = static_cast<QContactOnlineAccount*>(&newDetails[i]);
                    qDebug() << "orig" << acc->subTypes();
                    qDebug() << "new" << acc2->subTypes();
                }
            }
        }



        return true;
    } else {
        qDebug() << "Contact is equal";
        return false;
    }
}

bool QIndividual::update(const QString &vcard)
{
    QContact contact = VCardParser::vcardToContact(vcard);
    return update(contact);
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

} //namespace

