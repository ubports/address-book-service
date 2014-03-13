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

#ifndef __GALERA_ADDRESSBOOK_ADAPTOR_H__
#define __GALERA_ADDRESSBOOK_ADAPTOR_H__

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "common/source.h"
#include "common/dbus-service-defs.h"

namespace galera
{
class AddressBook;
class AddressBookAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", CPIM_ADDRESSBOOK_IFACE_NAME)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.pim.AddressBook\">\n"
"    <property name=\"isReady\" type=\"b\" access=\"read\"/>\n"
"    <signal name=\"contactsUpdated\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsRemoved\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsAdded\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"asyncOperationResult\">\n"
"      <arg direction=\"out\" type=\"a(ss)\" name=\"errorMap\"/>\n"
"    </signal>\n"
"    <signal name=\"ready\"/>\n"
"    <method name=\"ping\">\n"
"      <arg direction=\"out\" type=\"b\"/>\n"
"    </method>\n"
"    <method name=\"availableSources\">\n"
"      <arg direction=\"out\" type=\"a(ssb)\"/>\n"
"      <annotation value=\"SourceList\" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"source\">\n"
"      <arg direction=\"out\" type=\"(ssb)\"/>\n"
"      <annotation value=\"Source\" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"createSource\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"(sb)\"/>\n"
"      <annotation value=\"Source\" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"sortFields\">\n"
"      <arg direction=\"out\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"query\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"clause\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"sort\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"sources\"/>\n"
"      <arg direction=\"out\" type=\"o\"/>\n"
"    </method>\n"
"    <method name=\"removeContacts\">\n"
"      <arg direction=\"out\" type=\"i\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"contactIds\"/>\n"
"    </method>\n"
"    <method name=\"createContact\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"contact\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"source\"/>\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"updateContacts\">\n"
"      <arg direction=\"out\" type=\"as\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"contacts\"/>\n"
"    </method>\n"
"    <method name=\"linkContacts\">\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"contacts\"/>\n"
"    </method>\n"
"    <method name=\"unlinkContacts\">\n"
"      <arg direction=\"out\" type=\"b\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"parent\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"contacts\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
    Q_PROPERTY(bool isReady READ isReady NOTIFY ready)
public:
    AddressBookAdaptor(const QDBusConnection &connection, AddressBook *parent);
    virtual ~AddressBookAdaptor();

public Q_SLOTS:
    SourceList availableSources(const QDBusMessage &message);
    Source source(const QDBusMessage &message);
    Source createSource(const QString &sourceName, const QDBusMessage &message);
    QStringList sortFields();
    QDBusObjectPath query(const QString &clause, const QString &sort, const QStringList &sources);
    int removeContacts(const QStringList &contactIds, const QDBusMessage &message);
    QString createContact(const QString &contact, const QString &source, const QDBusMessage &message);
    QStringList updateContacts(const QStringList &contacts, const QDBusMessage &message);
    QString linkContacts(const QStringList &contacts);
    bool unlinkContacts(const QString &parentId, const QStringList &contactsIds);
    bool isReady();
    bool ping();

Q_SIGNALS:
    void contactsAdded(const QStringList &ids);
    void contactsRemoved(const QStringList &ids);
    void contactsUpdated(const QStringList &ids);
    void asyncOperationResult(QMap<QString, QString> errors);
    void ready();

private:
    AddressBook *m_addressBook;
    QDBusConnection m_connection;
};

} //namespace

#endif

