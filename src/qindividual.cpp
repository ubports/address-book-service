#include "qindividual.h"

#include <QVersitDocument>
#include <QVersitProperty>
#include <QVersitWriter>


using namespace QtVersit;
namespace galera
{

QIndividual::QIndividual(FolksIndividual *individual)
    : m_individual(individual)
{
    g_object_ref(m_individual);
}

QIndividual::~QIndividual()
{
    g_object_unref(m_individual);
}

QVersitProperty QIndividual::uid() const
{
    QVersitProperty prop;
    //UID
    prop.setName("UID");
    prop.setValue(folks_individual_get_id(m_individual));

    return prop;
}

PropertyList QIndividual::clientPidMap() const
{
    QList<QVersitProperty> clientPidMap;
    QVersitProperty prop;
    int index = 1;

    GeeSet *personas = folks_individual_get_personas(m_individual);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(personas));

    while(gee_iterator_next(iter)) {
        FolksPersona *persona = FOLKS_PERSONA(gee_iterator_get(iter));
        prop.clear();
        prop.setName("CLIENTPIDMAP");
        prop.setValue(QString("%1;%2").arg(index++)
                                      .arg(folks_persona_get_uid(persona)));
        clientPidMap << prop;
    }

    g_object_unref(iter);
    return clientPidMap;
}

/*
 *N:Stevenson;John;Philip,Paul;Dr.;Jr.,M.D.,A.C.P.
 */
QtVersit::QVersitProperty QIndividual::name() const
{
    QVersitProperty prop;
    QStringList names;
    FolksStructuredName *sn = folks_name_details_get_structured_name(FOLKS_NAME_DETAILS(m_individual));

    if (sn) {
        names << folks_structured_name_get_family_name(sn)
              << folks_structured_name_get_given_name(sn)
              << folks_structured_name_get_additional_names(sn)
              << folks_structured_name_get_prefixes(sn)
              << folks_structured_name_get_suffixes(sn);

        prop.setName("N");
        prop.setValueType(QVersitProperty::CompoundType);
        prop.setValue(names);
    }
    return prop;
}

/*
 *FN:Mr. John Q. Public\, Esq.
 */
QtVersit::QVersitProperty QIndividual::fullName() const
{
    QVersitProperty prop;
    prop.setName("FN");
    prop.setValue(QString::fromUtf8(folks_name_details_get_full_name(FOLKS_NAME_DETAILS(m_individual))));
    return prop;
}

/*
 *NICKNAME:Robbie
 */
QtVersit::QVersitProperty QIndividual::nickName() const
{
    QVersitProperty prop;
    QString nickName = QString::fromUtf8(folks_name_details_get_nickname(FOLKS_NAME_DETAILS(m_individual)));
    if (!nickName.isEmpty()) {
        prop.setName("NICKNAME");
        prop.setValue(nickName);
    }
    return prop;
}

/*
 *BDAY:19960415
 */
QtVersit::QVersitProperty QIndividual::birthday() const
{
    GDateTime* datetime = folks_birthday_details_get_birthday(FOLKS_BIRTHDAY_DETAILS(m_individual));
    QVersitProperty prop;
    if (datetime) {
        prop.setName("BDAY");
        prop.setValue(QString("%1%2%3").arg(g_date_time_get_year(datetime))
                                       .arg(g_date_time_get_month(datetime))
                                       .arg(g_date_time_get_day_of_month(datetime)));
    }
    return prop;
}

/*
 *PHOTO:data:image/jpeg;base64,MIICajCCAdOgAwIBAgICBEUwDQYJKoZIhv
 *       AQEEBQAwdzELMAkGA1UEBhMCVVMxLDAqBgNVBAoTI05ldHNjYXBlIENvbW11bm
 *       ljYXRpb25zIENvcnBvcmF0aW9uMRwwGgYDVQQLExNJbmZvcm1hdGlvbiBTeXN0
 *       <...remainder of base64-encoded data...>
 */
QtVersit::QVersitProperty QIndividual::photo() const
{
    //TODO
    return QVersitProperty();
}

/*
 *ROLE:Project Leader
 */
PropertyList QIndividual::roles() const
{
    GeeSet *roles = folks_role_details_get_roles(FOLKS_ROLE_DETAILS(m_individual));
    return parseFieldList("ROLE", roles);
}

/*
 *EMAIL;TYPE=work:jqpublic@xyz.example.com
 */
PropertyList QIndividual::emails() const
{
    GeeSet *emails = folks_email_details_get_email_addresses(FOLKS_EMAIL_DETAILS(m_individual));
    return parseFieldList("EMAIL", emails);
}

/*
 *TEL;VALUE=uri;PREF=1;TYPE="voice,home":tel:+1-555-555-5555;ext=5555
 */
PropertyList QIndividual::phones() const
{
    GeeSet *phones = folks_phone_details_get_phone_numbers(FOLKS_PHONE_DETAILS(m_individual));
    return parseFieldList("TEL", phones);
}

/*
 * ADR;GEO="geo:12.3457,78.910";LABEL="Mr. John Q. Public, Esq.\n
 *     Mail Drop: TNE QB\n123 Main Street\nAny Town, CA  91921-1234\n
 *     U.S.A.":;;123 Main Street;Any Town;CA;91921-1234;U.S.A.
 */
PropertyList QIndividual::addresses() const
{
    QList<QVersitProperty> result;
    QVersitProperty prop;
    QStringList values;
    GeeSet *addresses;
    addresses = folks_postal_address_details_get_postal_addresses(FOLKS_POSTAL_ADDRESS_DETAILS(m_individual));
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(addresses));

    while(gee_iterator_next(iter)) {
        values.clear();
        prop.clear();

        FolksAbstractFieldDetails *fdetails = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));
        FolksPostalAddress *addr = FOLKS_POSTAL_ADDRESS(folks_abstract_field_details_get_value(fdetails));
        values << QString::fromUtf8(folks_postal_address_get_po_box(addr))
               << QString::fromUtf8(folks_postal_address_get_street(addr))
               << QString::fromUtf8(folks_postal_address_get_locality(addr))
               << QString::fromUtf8(folks_postal_address_get_region(addr))
               << QString::fromUtf8(folks_postal_address_get_postal_code(addr))
               << QString::fromUtf8(folks_postal_address_get_country(addr));

        prop.setName("ADR");
        prop.setValue(values.join(";"));
        prop.setParameters(parseDetails(fdetails));
        result << prop;
        g_object_unref(fdetails);
    }
    g_object_unref (iter);
    return result;
}

/*
 *IMPP;TYPE=personal,pref:im:alice@example.com
 */
PropertyList QIndividual::ims() const
{
    QList<QVersitProperty> result;
    QVersitProperty prop;
    GeeMultiMap *ims = folks_im_details_get_im_addresses(FOLKS_IM_DETAILS(m_individual));
    GeeSet *keys = gee_multi_map_get_keys(ims);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(keys));

    while(gee_iterator_next(iter)) {
        const gchar *key = (const gchar*) gee_iterator_get(iter);
        GeeCollection *values = gee_multi_map_get(ims, key);
        GeeIterator *iterValues = gee_iterable_iterator(GEE_ITERABLE(values));

        while(gee_iterator_next(iterValues)) {
            FolksAbstractFieldDetails *fd = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iterValues));


            prop.clear();
            prop.setName("IMPP");
            prop.setValue(QString("%1:%2").arg(QString::fromUtf8(key))
                          .arg(QString::fromUtf8((const char*) folks_abstract_field_details_get_value(fd))));
            prop.setParameters(parseDetails(fd));
            result << prop;
        }
    }
    return result;
}

/*
 *TZ;VALUE=utc-offset:-0500
 *      ; Note: utc-offset format is NOT RECOMMENDED.
 */
QtVersit::QVersitProperty QIndividual::timeZone() const
{
    //TODO
    return QVersitProperty();
}

/*
 *URL:http://example.org/restaurant.french/~chezchic.html
 */
PropertyList QIndividual::urls() const
{
    GeeSet *urls = folks_url_details_get_urls(FOLKS_URL_DETAILS(m_individual));
    return parseFieldList("URL", urls);
}

PropertyList QIndividual::extra() const
{
    //TODO
    return PropertyList();
}

QList<QVersitProperty> QIndividual::parseFieldList(const QString &fieldName, GeeSet *values) const
{
    QList<QVersitProperty> result;
    QVersitProperty prop;
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(values));

    while(gee_iterator_next(iter)) {
        FolksAbstractFieldDetails *fdetails = FOLKS_ABSTRACT_FIELD_DETAILS(gee_iterator_get(iter));

        const gchar* value = (const gchar*) folks_abstract_field_details_get_value(fdetails);
        prop.clear();
        prop.setName(fieldName);
        prop.setValue(QString::fromUtf8(value));
        prop.setParameters(parseDetails(fdetails));

        result << prop;
        g_object_unref(fdetails);
     }
     g_object_unref (iter);
     return result;
}

QMultiHash<QString, QString> QIndividual::parseDetails(FolksAbstractFieldDetails *details) const
{
    QMultiHash<QString, QString> props;
    GeeMultiMap *parameters = folks_abstract_field_details_get_parameters(details);
    GeeSet *keys = gee_multi_map_get_keys(parameters);
    GeeIterator *iter = gee_iterable_iterator(GEE_ITERABLE(keys));

    while (gee_iterator_next(iter)) {
        gchar *gkey = (gchar*) gee_iterator_get(iter);
        QString key = QString::fromUtf8(gkey);

        GeeCollection *values = folks_abstract_field_details_get_parameter_values(details, gkey);
        GeeIterator *viter = gee_iterable_iterator(GEE_ITERABLE(values));
        while (gee_iterator_next(viter)) {
            QString value = QString::fromUtf8((gchar*) gee_iterator_get(viter));
            props.insertMulti(key, value);
        }
        g_object_unref(viter);
    }

    g_object_unref(iter);
    return props;
}

bool QIndividual::fieldsContains(Fields fields, Field value) const
{
    return (fields.testFlag(QIndividual::All) || fields.testFlag(value));
}

QString QIndividual::toString(Fields fields) const
{
    QVersitDocument vcard(QVersitDocument::VCard30Type);
    vcard.addProperty(uid());

    Q_FOREACH(QVersitProperty pidmap, clientPidMap()) {
        vcard.addProperty(pidmap);
    }

    if (fieldsContains(fields, QIndividual::Name)) {
        QVersitProperty n = name();
        if (!n.isEmpty()) {
            vcard.addProperty(n);
        }
    }

    if (fieldsContains(fields, QIndividual::FullName)) {
        QVersitProperty fn = fullName();
        if (!fn.isEmpty()) {
            vcard.addProperty(fn);
        }
    }

    if (fieldsContains(fields, QIndividual::NickName)) {
        QVersitProperty nickName = this->nickName();
        if (!nickName.isEmpty()) {
            vcard.addProperty(nickName);
        }
    }

    if (fieldsContains(fields, QIndividual::Birthday)) {
        QVersitProperty birthday = this->birthday();
        if (!birthday.isEmpty()) {
            vcard.addProperty(birthday);
        }
    }

    if (fieldsContains(fields, QIndividual::Photo)) {
        QVersitProperty photo = this->photo();
        if (!photo.isEmpty()) {
            vcard.addProperty(photo);
        }
    }

    if (fieldsContains(fields, QIndividual::Role)) {
        Q_FOREACH(QVersitProperty role, roles()) {
            vcard.addProperty(role);
        }
    }

    if (fieldsContains(fields, QIndividual::Email)) {
        Q_FOREACH(QVersitProperty email, emails()) {
            if (!email.isEmpty()) {
                vcard.addProperty(email);
            }
        }
    }

    if (fieldsContains(fields, QIndividual::Phone)) {
        Q_FOREACH(QVersitProperty phone, phones()) {
            if (!phone.isEmpty()) {
                vcard.addProperty(phone);
            }
        }
    }

    if (fieldsContains(fields, QIndividual::Address)) {
        Q_FOREACH(QVersitProperty addr, addresses()) {
            if (!addr.isEmpty()) {
                vcard.addProperty(addr);
            }
        }
    }

    if (fieldsContains(fields, QIndividual::Url)) {
        Q_FOREACH(QVersitProperty url, urls()) {
            if (!url.isEmpty()) {
                vcard.addProperty(url);
            }
        }
    }

    if (fieldsContains(fields, QIndividual::TimeZone)) {
        QVersitProperty tz = timeZone();
        if (!tz.isEmpty()) {
            vcard.addProperty(tz);
        }
    }

    QByteArray result;
    QVersitWriter writer(&result);
    writer.startWriting(vcard);
    writer.waitForFinished();

    return result;
}

} //namespace

