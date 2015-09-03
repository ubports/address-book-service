#pragma once
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkConfigurationManager>

class ABUpdateModule;

class ABUpdate : public QObject
{
    Q_OBJECT
public:
    ABUpdate(QObject *parent = 0);
    ~ABUpdate() override;

    QList<ABUpdateModule*> needsUpdate() const;
    bool isRunning();

    // test
    void skipNetworkTest();

public Q_SLOTS:
    void startUpdate();
    void cancelUpdate();

Q_SIGNALS:
    void onNeedsUpdateChanged();
    void onIsRunningChanged();
    void updateDone();
    void updateError(const QString &errorMessage);

private Q_SLOTS:
    void onModuleUpdated();
    void onModuleUpdateError(const QString &errorMessage);
    void onOnlineStateChanged(bool isOnline);

private:
    QScopedPointer<QNetworkConfigurationManager> m_netManager;
    QList<ABUpdateModule*> m_updateModules;
    QList<ABUpdateModule*> m_modulesToUpdate;
    QMutex m_lock;
    bool m_needsUpdate;
    bool m_isRunning;
    bool m_waitingForIntenert;
    int m_activeModule;
    bool m_skipNetworkTest;

    void notifyDone();
    void startUpdate(ABUpdateModule *module);
    void notifyError(ABUpdateModule *module);
    bool isOnline() const;
    void waitForInternet();
    void updateNextModule();
};
