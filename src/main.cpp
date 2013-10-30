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

#include "addressbook.h"
#include "view.h"
#include "source.h"
#include "common/dbus-service-defs.h"

void contactServiceMessageOutput(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &message)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    Q_UNUSED(message);
    //nothing
}


int main(int argc, char** argv)
{
    galera::AddressBook::init();
    galera::Source::registerMetaType();
    QCoreApplication app(argc, argv);

    // disable debug message if variable not exported
    if (qgetenv("ADDRESS_BOOK_SERVICE_DEBUG").isEmpty()) {
        qInstallMessageHandler(contactServiceMessageOutput);
    }

    galera::AddressBook book;
    book.start();
    app.connect(&book, SIGNAL(stopped()), SLOT(quit()));

    return app.exec();
}

