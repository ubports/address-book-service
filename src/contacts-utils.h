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

#ifndef __GALERA_CONTACTS_UTILS_H__
#define __GALERA_CONTACTS_UTILS_H__

#include <QtCore/QString>
#include <QVersitProperty>

#include <folks/folks.h>

namespace galera
{

class ContactsUtils
{
public:
    static QByteArray serializeIndividual(FolksIndividual *individual);

private:
    static QList<QtVersit::QVersitProperty> parsePersona(int index, FolksPersona *persona);
    static QtVersit::QVersitProperty createProperty(int sourceIndex,
                                                    int index,
                                                    const QString &name,
                                                    const QString &value);
};

} //namespace
#endif
