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

#ifndef __GALERA_DETAIL_CONTEXT_PARSER_H__
#define __GALERA_DETAIL_CONTEXT_PARSER_H__

#include <QtCore/QStringList>

#include <QtContacts/QContactDetail>

#include <folks/folks.h>

namespace galera {

class DetailContextParser
{
public:
    static void parseContext(FolksAbstractFieldDetails *fd, const QtContacts::QContactDetail &detail, bool isPreffered);
    static QStringList parseContext(const QtContacts::QContactDetail &detail);
    static QStringList listContext(const QtContacts::QContactDetail &detail);
    static QStringList parsePhoneContext(const QtContacts::QContactDetail &detail);
    static QStringList parseAddressContext(const QtContacts::QContactDetail &detail);
    static QStringList parseOnlineAccountContext(const QtContacts::QContactDetail &im);
    static QString accountProtocolName(int protocol);

    static QStringList listParameters(FolksAbstractFieldDetails *details);
    static void parseParameters(QtContacts::QContactDetail &detail, FolksAbstractFieldDetails *fd, bool *isPref);
    static QList<int> contextsFromParameters(QStringList &parameters);
    static void parsePhoneParameters(QtContacts::QContactDetail &phone, const QStringList &params);
    static void parseAddressParameters(QtContacts::QContactDetail &address, const QStringList &parameters);
    static int accountProtocolFromString(const QString &protocol);
    static void parseOnlineAccountParameters(QtContacts::QContactDetail &im, const QStringList &parameters);
};

}

#endif
