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

#include "detail-context-parser.h"

#include "common/vcard-parser.h"

#include <QtCore/QMap>

#include <QtContacts/qcontactdetails.h>

using namespace QtContacts;

namespace galera {

void DetailContextParser::parseContext(FolksAbstractFieldDetails *fd,
                                       const QtContacts::QContactDetail &detail,
                                       bool isPreffered)
{
    // clear the current values to prevent duplicate values
    folks_abstract_field_details_set_parameter(fd, "type", "");

    Q_FOREACH(const QString &param, listContext(detail)) {
        folks_abstract_field_details_add_parameter(fd, "type", param.toUtf8().data());
    }

    if (isPreffered) {
        folks_abstract_field_details_set_parameter(fd, VCardParser::PrefParamName.toLower().toUtf8().data(), "1");
    }
}

QStringList DetailContextParser::parseContext(const QtContacts::QContactDetail &detail)
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

QStringList DetailContextParser::listContext(const QtContacts::QContactDetail &detail)
{
    QStringList context = parseContext(detail);
    switch (detail.type()) {
        case QContactDetail::TypePhoneNumber:
            context << parsePhoneContext(detail);
            break;
        case QContactDetail::TypeAddress:
            context << parseAddressContext(detail);
            // Setting more than one context causes folks to freeze
            if (!context.isEmpty()) {
                return context.mid(0,1);
            }
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

QStringList DetailContextParser::parsePhoneContext(const QtContacts::QContactDetail &detail)
{
    static QMap<int, QString> map;

    // populate the map once
    if (map.isEmpty()) {
        map[QContactPhoneNumber::SubTypeLandline] = "landline";
        map[QContactPhoneNumber::SubTypeMobile] = "cell";
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

QStringList DetailContextParser::parseAddressContext(const QtContacts::QContactDetail &detail)
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

QStringList DetailContextParser::parseOnlineAccountContext(const QtContacts::QContactDetail &im)
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

QString DetailContextParser::accountProtocolName(int protocol)
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

QStringList DetailContextParser::listParameters(FolksAbstractFieldDetails *details)
{
    static QStringList whiteList;

    if (whiteList.isEmpty()) {
        whiteList << "x-evolution-ui-slot"
                  << "x-google-label";
    }
    GeeMultiMap *map = folks_abstract_field_details_get_parameters(details);
    GeeSet *keys = gee_multi_map_get_keys(map);
    GeeIterator *siter = gee_iterable_iterator(GEE_ITERABLE(keys));

    QStringList params;

    while (gee_iterator_next (siter)) {
        char *parameter = (char*) gee_iterator_get(siter);
        if (QString::fromUtf8(parameter).toUpper() == VCardParser::PrefParamName) {
            params << "pref";
            continue;
        } else if (QString::fromUtf8(parameter) != "type") {
            if (!whiteList.contains(QString::fromUtf8(parameter))) {
                qDebug() << "not supported field details" << parameter;
                // FIXME: check what to do with other parameters
            }
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

        g_free(parameter);
        g_object_unref(iter);
    }

    g_object_unref(siter);
    return params;
}

void DetailContextParser::parseParameters(QtContacts::QContactDetail &detail,
                                          FolksAbstractFieldDetails *fd,
                                          bool *isPref)
{
    QStringList params = listParameters(fd);

    if (isPref) {
        *isPref = params.contains(VCardParser::PrefParamName.toLower());
        if (*isPref) {
            params.removeOne(VCardParser::PrefParamName.toLower());
        }
    }
    QList<int> context = contextsFromParameters(&params);
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

QList<int> DetailContextParser::contextsFromParameters(QStringList *parameters)
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
    Q_FOREACH(const QString &param, *parameters) {
        if (map.contains(param.toLower())) {
            accepted << param;
            values << map[param.toLower()];
        }
    }

    Q_FOREACH(const QString &param, accepted) {
        parameters->removeOne(param);
    }

    return values;
}

void DetailContextParser::parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &params)
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
        } else if (!param.isEmpty()) {
            qWarning() << "Invalid phone parameter:" << param;
        }
    }

    if (!subTypes.isEmpty()) {
        static_cast<QContactPhoneNumber*>(&phone)->setSubTypes(subTypes);
    }
}

void DetailContextParser::parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters)
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

void DetailContextParser::parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters)
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

int DetailContextParser::accountProtocolFromString(const QString &protocol)
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

} // namespace
