#pragma once
#include <QtCore/QObject>
#include <QtDBus/QDBusAbstractAdaptor>

#include "ab-update.h"
#include "dbus-service-defs.h"
#include "config.h"

class ABUpdateAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", CPIM_UPDATE_OBJECT_IFACE_NAME)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.pim.Updater\">\n"
"    <property name=\"needsUpdate\" type=\"b\" access=\"read\"/>\n"
"    <property name=\"isRunning\" type=\"b\" access=\"read\"/>\n"
"    <signal name=\"updateDone\"/>\n"
"    <signal name=\"updateError\">\n"
"      <arg direction=\"out\" type=\"s\" name=\"errorMessage\"/>\n"
"    </signal>\n"
"    <method name=\"startUpdate\"/>\n"
"    <method name=\"cancelUpdate\"/>\n"
"  </interface>\n"
"")
    Q_PROPERTY(bool needsUpdate READ needsUpdate NOTIFY onNeedsUpdateChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY onIsRunningChanged)

public:
    ABUpdateAdaptor(ABUpdate *parent = 0);

    bool needsUpdate() const;
    bool isRunning() const;

public Q_SLOTS:
    void startUpdate();
    void cancelUpdate();

Q_SIGNALS:
    void onNeedsUpdateChanged();
    void onIsRunningChanged();
    void updateDone();
    void updateError(const QString &errorMessage);

private:
    ABUpdate *m_abUpdate;
    bool m_needsUpdate;
    bool m_isRunning;
};
