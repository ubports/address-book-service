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
