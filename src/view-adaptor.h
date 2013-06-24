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

#ifndef __GALERA_VIEW_ADAPTOR_H__
#define __GALERA_VIEW_ADAPTOR_H__

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtDBus/QtDBus>

#include "common/dbus-service-defs.h"

namespace galera
{

class View;
class ViewAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", CPIM_ADDRESSBOOK_VIEW_IFACE_NAME)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.pim.AddressBookView\">\n"
"    <signal name=\"contactsUpdated\">\n"
"      <arg direction=\"out\" type=\"i\" name=\"pos\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"lenght\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsRemoved\">\n"
"      <arg direction=\"out\" type=\"i\" name=\"pos\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"lenght\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsAdded\">\n"
"      <arg direction=\"out\" type=\"i\" name=\"pos\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"lenght\"/>\n"
"    </signal>\n"
"    <method name=\"sort\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"field\"/>\n"
"    </method>\n"
"    <method name=\"count\">\n"
"      <arg direction=\"out\" type=\"i\"/>\n"
"    </method>\n"
"    <method name=\"contactsDetails\">\n"
"      <arg direction=\"in\" type=\"as\" name=\"fields\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"startIndex\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"pageSize\"/>\n"
"      <arg direction=\"out\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"contactDetails\">\n"
"      <arg direction=\"in\" type=\"as\" name=\"fields\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"id\"/>\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"close\"/>\n"
"  </interface>\n"
        "")
public:
    ViewAdaptor(const QDBusConnection &connection, View *parent);
    virtual ~ViewAdaptor();

public Q_SLOTS:
    QString contactDetails(const QStringList &fields, const QString &id);
    QStringList contactsDetails(const QStringList &fields, int startIndex, int pageSize, const QDBusMessage &message);
    int count();
    void sort(const QString &field);
    void close();

Q_SIGNALS:
    void contactsAdded(int pos, int lenght);
    void contactsRemoved(int pos, int lenght);
    void contactsUpdated(int pos, int lenght);

private:
    View *m_view;
    QDBusConnection m_connection;
};

} // namespace

#endif

