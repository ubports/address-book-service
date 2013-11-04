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

#include "scoped-loop.h"

ScopedEventLoop::ScopedEventLoop(QEventLoop **proxy)
{
    reset(proxy);
}

ScopedEventLoop::~ScopedEventLoop()
{
    *m_proxy = 0;
}

void ScopedEventLoop::reset(QEventLoop **proxy)
{
    *proxy = &m_eventLoop;
    m_proxy = proxy;
}

void ScopedEventLoop::exec()
{
    if (*m_proxy) {
        m_eventLoop.exec();
        *m_proxy = 0;
    }
}
