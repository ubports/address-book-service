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

#ifndef __GALERA_VCARD_PARSER_H__
#define __GALERA_VCARD_PARSER_H__

#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtContacts/QtContacts>

namespace galera
{

class VCardParser
{
public:

    static QStringList contactToVcard(QList<QtContacts::QContact> contacts);
    static QtContacts::QContact vcardToContact(const QString &vcard);
    static QList<QtContacts::QContact> vcardToContact(const QStringList &vcardList);
};

}

#endif
