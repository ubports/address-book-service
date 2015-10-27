/*
 * Copyright 2015 Canonical Ltd.
 *
 * This file is part of address-book-service.
 *
 * sync-monitor is free software; you can redistribute it and/or modify
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

#include <QObject>
#include <QTemporaryFile>
#include <QSettings>
#include <QDebug>
#include <QSignalSpy>
#include <QTest>

#include "ab-update.h"

class TstButeoImportNoAccounts : public QObject
{
    Q_OBJECT

    QTemporaryFile *m_settingsFile;

private Q_SLOTS:
    void init()
    {
        m_settingsFile = new QTemporaryFile;
        QVERIFY(m_settingsFile->open());
        qDebug() << "Using as temporary file:" << m_settingsFile->fileName();
        //Make sure that we use a new settings every time that we run the test
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsFile->fileName());
    }

    void cleanup()
    {
        delete m_settingsFile;
    }

    void tst_isOutDated()
    {
        ABUpdate updater;
        updater.skipNetworkTest();
        updater.setSilenceMode(true);

        QVERIFY(updater.needsUpdate().isEmpty());
    }

    void tst_importAccount()
    {
        ABUpdate updater;
        updater.skipNetworkTest();

        QSignalSpy updatedSignal(&updater, SIGNAL(updateDone()));

        qDebug() << "WILL START";
        updater.startUpdate();
        QTRY_COMPARE(updatedSignal.count(), 1);
        QCOMPARE(updater.isRunning(), false);
        QVERIFY(updater.needsUpdate().isEmpty());
    }
};

QTEST_MAIN(TstButeoImportNoAccounts)

#include "tst-buteo-import-no-accounts.moc"
