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

#include "ab-notify-message.h"
#include "ab-i18n.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

int ABNotifyMessage::m_instanceCount = 0;

ABNotifyMessage::ABNotifyMessage(bool singleMessage, QObject *parent)
    : QObject(parent),
      m_notification(0),
      m_singleMessage(singleMessage)
{
    if (m_instanceCount == 0) {
        m_instanceCount++;
        notify_init(QCoreApplication::instance()->applicationName().toUtf8());
    }
}

ABNotifyMessage::~ABNotifyMessage()
{
    if (m_notification) {
        g_object_unref(m_notification);
        m_notification = 0;
    }

    m_instanceCount--;
    if (m_instanceCount == 0) {
        notify_uninit();
    }

}

void ABNotifyMessage::show(const QString &title, const QString &msg, const QString &iconName)
{
    m_notification = notify_notification_new(title.toUtf8().data(),
                                             msg.toUtf8().data(),
                                             iconName.isEmpty() ? (const char*) 0 : iconName.toUtf8().constData());

    GError *error = 0;
    notify_notification_set_timeout(m_notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_show(m_notification, &error);
    if (error) {
        qWarning() << "Fail to launch notification" << error->message;
        g_error_free(error);
    }
    g_signal_connect_after(m_notification,
                           "closed",
                           (GCallback) ABNotifyMessage::onNotificationClosed,
                           this);
}

void ABNotifyMessage::askYesOrNo(const QString &title,
                                 const QString &msg,
                                 const QString &iconName)
{
    m_notification = notify_notification_new(title.toUtf8().data(),
                                             msg.toUtf8().data(),
                                             iconName.isEmpty() ? (const char*) 0 : iconName.toUtf8().constData());
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-snap-decisions",
                                        "true");
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-private-button-tint",
                                        "true");
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-non-shaped-icon",
                                        "true");
    notify_notification_add_action(m_notification,
                                   "action_accept", _("Yes"),
                                   (NotifyActionCallback) ABNotifyMessage::onQuestionAccepted,
                                   this,
                                   NULL);
    notify_notification_add_action(m_notification,
                                   "action_reject", _("No"),
                                   (NotifyActionCallback) ABNotifyMessage::onQuestionRejected,
                                   this,
                                   NULL);
    notify_notification_show(m_notification, 0);
    g_signal_connect_after(m_notification,
                           "closed",
                           (GCallback) ABNotifyMessage::onNotificationClosed,
                           this);
}

void ABNotifyMessage::onQuestionAccepted(NotifyNotification *notification, char *action, ABNotifyMessage *self)
{
    Q_UNUSED(notification)
    Q_UNUSED(action)

    Q_EMIT self->questionAccepted();
}

void ABNotifyMessage::onQuestionRejected(NotifyNotification *notification, char *action, ABNotifyMessage *self)
{
    Q_UNUSED(notification)
    Q_UNUSED(action)

    Q_EMIT self->questionRejected();
}

void ABNotifyMessage::onNotificationClosed(NotifyNotification *notification, ABNotifyMessage *self)
{
    Q_EMIT self->messageClosed();
    if (self->m_singleMessage) {
        self->deleteLater();
    }
}
