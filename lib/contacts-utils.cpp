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

#include "contacts-utils.h"

#include <QVersitDocument>
#include <QVersitProperty>
#include <QVersitWriter>


using namespace QtVersit;
namespace galera
{

QVersitProperty ContactsUtils::createProperty(int sourceIndex,
                                              int index,
                                              const QString &name,
                                              const QString &value)
{
    QVersitProperty prop;
    QMultiHash<QString, QString> param;

    prop.setName(name);
    prop.setValue(value);
    param.insert("PID", QString("%1.%1").arg(sourceIndex).arg(index));
    prop.setParameters(param);
    return prop;
}

QList<QVersitProperty> ContactsUtils::parsePersona(int index, FolksPersona *persona)
{
    QList<QVersitProperty> result;
    QVersitProperty prop;

    prop.setName("CLIENTPIDMAP");
    prop.setValue(QString("%1;%2").arg(index)
                                  .arg(folks_persona_get_uid(persona)));
    result << prop;
    prop.clear();
    result << createProperty(index, 1, "N", folks_persona_(persona));
    return result;
}

QByteArray ContactsUtils::serializeIndividual(FolksIndividual *individual)
{
    QVersitProperty prop;
    QVersitDocument vcard(QVersitDocument::VCard30Type);

    //UID
    prop.setName("UID");
    prop.setValue(folks_individual_get_id(individual));
    vcard.addProperty(prop);

    GeeIterator *iter;
    GeeSet *personas = folks_individual_get_personas(individual);

    int index = 1;
    iter = gee_iterable_iterator(GEE_ITERABLE(personas));
    QList<QVersitProperty> personaProps;

    while(gee_iterator_next(iter)) {
        personaProps = parsePersona(index++, FOLKS_PERSONA(gee_iterator_get(iter)));
        Q_FOREACH(QVersitProperty p, personaProps) {
            vcard.addProperty(p);
        }
    }

    QByteArray result;
    QVersitWriter writer(&result);
    writer.startWriting(vcard);
    writer.waitForFinished();

    return result;
}

} //namespace

