#ifndef __GALERA_ADDRESSBOOK_ADAPTOR_H__
#define __GALERA_ADDRESSBOOK_ADAPTOR_H__

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "source.h"

namespace galera
{
class AddressBook;
class AddressBookAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.galera.AddressBook")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.galera.AddressBook\">\n"
"    <signal name=\"contactsUpdated\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsRemoved\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"contactsCreated\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"ids\"/>\n"
"    </signal>\n"
"    <signal name=\"asyncOperationResult\">\n"
"      <arg direction=\"out\" type=\"a(ss)\" name=\"errorMap\"/>\n"
"    </signal>\n"
"    <method name=\"availableSources\">\n"
"      <arg direction=\"out\" type=\"a(sb)\"/>\n"
"      <annotation value=\"SourceList\" name=\"com.trolltech.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"source\">\n"
"      <arg direction=\"out\" type=\"a(sb)\"/>\n"
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
"      <arg direction=\"out\" type=\"b\"/>\n"
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
public:
    AddressBookAdaptor(const QDBusConnection &connection, AddressBook *parent);
    virtual ~AddressBookAdaptor();

public Q_SLOTS:
    SourceList availableSources();
    QString createContact(const QString &contact, const QString &source, const QDBusMessage &message);
    QString linkContacts(const QStringList &contacts);
    QDBusObjectPath query(const QString &clause, const QString &sort, const QStringList &sources);
    bool removeContacts(const QStringList &contactIds);
    QStringList sortFields();
    bool unlinkContacts(const QString &parentId, const QStringList &contactsIds);
    QStringList updateContacts(const QStringList &contacts, const QDBusMessage &message);

Q_SIGNALS:
    void contactsCreated(const QStringList &ids);
    void contactsRemoved(const QStringList &ids);
    void contactsUpdated(const QStringList &ids);
    void asyncOperationResult(QMap<QString, QString> errors);

private:
    AddressBook *m_addressBook;
    QDBusConnection m_connection;
};

} //namespace

#endif

