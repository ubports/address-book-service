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

#include "src/addressbook.h"

#include <QObject>
#include <QEventLoop>

//#include <QtContacts>

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
//    QtContacts::QContactManager *m_contactManager;
    galera::AddressBook *m_addressBook;

//    QString createContact(const QtContacts::QContact &qcontact);
    void startService();
    void startServiceSync();

private:
    DummyfBackend *m_backend;
    FolksBackendStore *m_backendStore;
    DummyfPersonaStore *m_primaryPersonaStore;
    QEventLoop *m_eventLoop;

    static void checkError(GError *error);
    void configurePrimaryStore();


    static void backendEnabled(FolksBackendStore *backendStore,
                               GAsyncResult *res,
                               BaseDummyTest *self);

    static void backendStoreLoaded(FolksBackendStore *backendStore,
                                   GAsyncResult *res,
                                   BaseDummyTest *self);

//    static void individualAggregatorPrepared(FolksIndividualAggregator *fia,
//                                             GAsyncResult *res,
//                                             BaseDummyTest *self);

//    static void individualAggregatorAddedPersona(FolksIndividualAggregator *fia,
//                                                 GAsyncResult *res,
//                                                 BaseDummyTest *self);
protected Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
};

#endif
