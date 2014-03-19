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

#ifndef __DUMMY_BACKEND_TEST_H__
#define __DUMMY_BACKEND_TEST_H__

#include "dummy-backend-defs.h"

#include "lib/qindividual.h"

#include <QtCore/QObject>
#include <QtCore/QEventLoop>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtContacts/QContact>

#include <folks/folks.h>
#include <folks-dummy.h>

class DummyBackendAdaptor;

class DummyBackendProxy: public QObject
{
    Q_OBJECT
public:
    DummyBackendProxy();
    ~DummyBackendProxy();

    void start(bool useDBus = false);
    bool isReady() const;

    QString createContact(const QtContacts::QContact &qcontact);
    QString updateContact(const QString &contactId, const QtContacts::QContact &qcontact);
    QList<QtContacts::QContact> contacts() const;
    QList<galera::QIndividual*> individuals() const;

    FolksIndividualAggregator *aggregator() const;

public Q_SLOTS:
    void shutdown();
    QStringList listContacts() const;
    void reset();
    void contactUpdated(const QString &contactId, const QString &errorMsg);

Q_SIGNALS:
    void ready();
    void stopped();

private:
    QTemporaryDir m_tmpDir;
    DummyBackendAdaptor *m_adaptor;
    FolksDummyBackend *m_backend;
    FolksDummyPersonaStore *m_primaryPersonaStore;
    FolksBackendStore *m_backendStore;
    QEventLoop *m_eventLoop;
    FolksIndividualAggregator *m_aggregator;
    bool m_isReady;
    int m_individualsChangedDetailedId;
    QHash<QString, galera::QIndividual*> m_contacts;
    bool m_contactUpdated;
    bool m_useDBus;

    bool registerObject();
    void initFolks();
    void configurePrimaryStore();
    void initEnviroment();
    void prepareAggregator();
    void mkpath(const QString &path) const;
    static void checkError(GError *error);
    static void backendEnabled(FolksBackendStore *backendStore,
                               GAsyncResult *res,
                               DummyBackendProxy *self);
    static void backendStoreLoaded(FolksBackendStore *backendStore,
                                   GAsyncResult *res,
                                   DummyBackendProxy *self);
    static void individualAggregatorPrepared(FolksIndividualAggregator *fia,
                                             GAsyncResult *res,
                                             DummyBackendProxy *self);
    static void individualAggregatorAddedPersona(FolksIndividualAggregator *fia,
                                                 GAsyncResult *res,
                                                 DummyBackendProxy *self);
    static void individualsChangedCb(FolksIndividualAggregator *individualAggregator,
                                     GeeMultiMap *changes,
                                     DummyBackendProxy *self);
};

class DummyBackendAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DUMMY_IFACE_NAME)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.test.folks.Dummy\">\n"
"    <property name=\"isReady\" type=\"b\" access=\"read\"/>\n"
"    <signal name=\"ready\"/>\n"
"    <method name=\"ping\">\n"
"      <arg direction=\"out\" type=\"b\"/>\n"
"    </method>\n"
"    <method name=\"quit\"/>\n"
"    <method name=\"enableAutoLink\">\n"
"      <arg direction=\"in\" type=\"b\"/>\n"
"    </method>\n"
"    <method name=\"createContact\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"updateContact\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"listContacts\">\n"
"      <arg direction=\"out\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"reset\"/>\n"
"  </interface>\n"
        "")
    Q_PROPERTY(bool isReady READ isReady NOTIFY ready)
public:
    DummyBackendAdaptor(const QDBusConnection &connection, DummyBackendProxy *parent);
    virtual ~DummyBackendAdaptor();
    bool isReady();

public Q_SLOTS:
    bool ping();
    void quit();
    void reset();
    QStringList listContacts();
    QString createContact(const QString &vcard);
    QString updateContact(const QString &contactId, const QString &vcard);
    void enableAutoLink(bool flag);

Q_SIGNALS:
    void ready();
    void stopped();

private:
    QDBusConnection m_connection;
    DummyBackendProxy *m_proxy;
};

#endif
