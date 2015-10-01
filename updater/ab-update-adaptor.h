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
    ~ABUpdateAdaptor();

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
