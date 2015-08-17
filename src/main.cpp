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

#define ADDRESS_BOOK_SERVICE_DEMO_DATA  "ADDRESS_BOOK_SERVICE_DEMO_DATA"
#define ADDRESS_BOOK_SERVICE_DEBUG      "ADDRESS_BOOK_SERVICE_DEBUG"

#include "config.h"
#include "addressbook.h"
#include "common/vcard-parser.h"

#include <QtCore/QSettings>

void contactServiceMessageOutput(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &message)
{
    if (type != QtDebugMsg) {
        printf("[%s:%d]%s\n", qPrintable(QFileInfo(context.file).fileName()), context.line, qPrintable(message));
    }
}

void onServiceReady(galera::AddressBook *book)
{
    // disable debug message if variable not exported
    if (qEnvironmentVariableIsSet(ADDRESS_BOOK_SERVICE_DEMO_DATA)) {
        QFile demoFileData(qgetenv(ADDRESS_BOOK_SERVICE_DEMO_DATA));
        qDebug() << "Load demo data from:" << demoFileData.fileName();
        if (demoFileData.open(QFile::ReadOnly)) {
            QByteArray demoData = demoFileData.readAll();
            QStringList vcards = galera::VCardParser::splitVcards(demoData);
            Q_FOREACH(const QString &vcard, vcards) {
                book->createContact(vcard, "");
            }
        }
    }
}

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName("Canonical");
    QCoreApplication::setApplicationName("Address Book Service");

    galera::AddressBook::init();
    QCoreApplication app(argc, argv);

    // disable debug message if variable not exported
    if (qgetenv(ADDRESS_BOOK_SERVICE_DEBUG).isEmpty()) {
        qInstallMessageHandler(contactServiceMessageOutput);
    }

    // disable folks linking for now
    if (!qEnvironmentVariableIsSet("FOLKS_DISABLE_LINKING")) {
        qputenv("FOLKS_DISABLE_LINKING", "on");
    }

    // Set primary store manually to avoid problems when removing stores
    //
    // Without manually set the primary store folks will try go guess that, and affter adding
    // a new store, folks will check if the eds store is  marked as default, and if yes then folks
    // will update the primary store pointer with this new store.
    // But after removing a source if the source is used as the primary by folks,
    // folks will set the primary pointer to null.
    if (!qEnvironmentVariableIsSet("FOLKS_PRIMARY_STORE")) {
        qputenv("FOLKS_PRIMARY_STORE", "eds:system-address-book");
    }

    galera::AddressBook book;
    QObject::connect(&book, &galera::AddressBook::readyChanged, [&book] () { onServiceReady(&book); });
    app.connect(&book, SIGNAL(stopped()), SLOT(quit()));

    if (book.start()) {
        qDebug() << "Service started";
        return app.exec();
    } else {
        qDebug() << "Fail to start service";
    }
}

