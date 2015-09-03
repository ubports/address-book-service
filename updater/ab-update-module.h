#pragma once
#include <QtCore/QObject>

class ABUpdateModule : public QObject
{
    Q_OBJECT
    Q_ENUMS(ImportError)

public:
    enum ImportError {
        ApplicationAreadyUpdated = 0,
        FailToConnectWithButeo,
        FailToCreateButeoProfiles,
        InernalError,
        OnlineAccountNotFound,
        SyncAlreadyRunning,
        SyncError
    };

    ABUpdateModule(QObject *parent = 0);
    ~ABUpdateModule() override;

    virtual QString name() const = 0;
    virtual bool needsUpdate() = 0;
    virtual bool update() = 0;
    virtual bool canUpdate() = 0;
    virtual bool requireInternetConnection() = 0;
    virtual bool commit() = 0;
    virtual bool roolback() = 0;
    virtual ImportError lastError() const = 0;

Q_SIGNALS:
    void updated();
    void updateError(ImportError errorCode);
};
