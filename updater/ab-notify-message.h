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
#include <QtCore/QString>
#include <QtCore/QObject>

#include <libnotify/notify.h>

class ABNotifyMessage : public QObject
{
    Q_OBJECT
public:
    ABNotifyMessage(bool singleMessage, QObject *parent = 0);
    ~ABNotifyMessage();

    void show(const QString &title, const QString &msg, const QString &iconName);
    void askYesOrNo(const QString &title, const QString &msg, const QString &iconName);
    int closedReason() const;

Q_SIGNALS:
    void questionAccepted();
    void questionRejected();
    void messageClosed();

private:
    NotifyNotification *m_notification;
    bool m_singleMessage;
    static int m_instanceCount;

    static void onQuestionAccepted(NotifyNotification *notification,
                                   char *action,
                                   ABNotifyMessage *self);
    static void onQuestionRejected(NotifyNotification *notification,
                                   char *action,
                                   ABNotifyMessage *self);
    static void onNotificationClosed(NotifyNotification *notification,
                                     ABNotifyMessage *self);
};
