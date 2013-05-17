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

#include "source.h"

namespace galera {

Source::Source()
    : m_isReadOnly(false)
{
}

Source::Source(const Source &other)
    : m_id(other.id()),
      m_isReadOnly(other.isReadOnly())
{
}

Source::Source(QString id, bool isReadOnly)
    : m_id(id),
      m_isReadOnly(isReadOnly)
{
}

bool Source::isValid() const
{
    return !m_id.isEmpty();
}

QString Source::id() const
{
    return m_id;
}

bool Source::isReadOnly() const
{
    return m_isReadOnly;
}

void Source::registerMetaType()
{
    qRegisterMetaType<Source>("Source");
    qRegisterMetaType<SourceList>("SourceList");
    qDBusRegisterMetaType<Source>();
    qDBusRegisterMetaType<SourceList>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Source &source)
{
    argument.beginStructure();
    argument << source.m_id;
    argument << source.m_isReadOnly;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Source &source)
{
    argument.beginStructure();
    argument >> source.m_id;
    argument >> source.m_isReadOnly;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SourceList &sources)
{
    argument.beginArray(qMetaTypeId<Source>());
    for(int i=0; i < sources.count(); ++i) {
        argument << sources[i];
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SourceList &sources)
{
    qDebug() << "PAser list of sources";
    argument.beginArray();
    sources.clear();
    while(!argument.atEnd()) {
        Source source;
        argument >> source;
        sources << source;
    }
    argument.endArray();
    return argument;
}

} // namespace Galera



