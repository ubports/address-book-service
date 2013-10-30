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

#ifndef __BASE_DUMMY_TEST_H__
#define __BASE_DUMMY_TEST_H__

#include "lib/addressbook.h"

#include <QObject>
#include <QEventLoop>

#include <folks-dummy.h>

class ScopedEventLoop
{
public:
    ScopedEventLoop(QEventLoop **proxy);
    ~ScopedEventLoop();
    void reset(QEventLoop **proxy);
    void exec();

private:
    QEventLoop m_eventLoop;
    QEventLoop **m_proxy;
};

class BaseDummyTest : public QObject
{
    Q_OBJECT

protected:
    galera::AddressBook *m_addressBook;

    QString createContact(const QtContacts::QContact &qcontact);
    void startService();
    void startServiceSync();
    void initEnviroment();

private:
    DummyfBackend *m_backend;
    DummyfPersonaStore *m_primaryPersonaStore;
    FolksBackendStore *m_backendStore;
    QEventLoop *m_eventLoop;
    QTemporaryDir m_tmpDir;

    void configurePrimaryStore();

    static void mkpath(const QString &path);
    static void checkError(GError *error);
    static void backendEnabled(FolksBackendStore *backendStore,
                               GAsyncResult *res,
                               BaseDummyTest *self);
    static void backendStoreLoaded(FolksBackendStore *backendStore,
                                   GAsyncResult *res,
                                   BaseDummyTest *self);
    static void individualAggregatorPrepared(FolksIndividualAggregator *fia,
                                             GAsyncResult *res,
                                             BaseDummyTest *self);
    static void individualAggregatorAddedPersona(FolksIndividualAggregator *fia,
                                                 GAsyncResult *res,
                                                 BaseDummyTest *self);
protected Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
};

#endif
