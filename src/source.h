/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
 *
 * ontact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * webbrowser-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GALERA_SOURCE_H__
#define __GALERA_SOURCE_H__

#include <QString>
#include <QtDBus/QtDBus>

namespace galera {

class Source
{
public:
    Source();
    Source(const Source &other);
    Source(QString id, bool isReadOnly);
    friend QDBusArgument &operator<<(QDBusArgument &argument, const Source &source);
    friend const QDBusArgument &operator>>(const QDBusArgument &argument, Source &source);

    static void registerMetaType();
    QString id() const;
    bool isReadOnly() const;
    bool isValid() const;

private:
    QString m_id;
    bool m_isReadOnly;
};

typedef QList<Source> SourceList;

QDBusArgument &operator<<(QDBusArgument &argument, const SourceList &sources);
const QDBusArgument &operator>>(const QDBusArgument &argument, SourceList &sources);

} // namespace galera

Q_DECLARE_METATYPE(galera::Source)
Q_DECLARE_METATYPE(galera::SourceList)

#endif

