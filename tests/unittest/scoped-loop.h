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

#ifndef __SCOPED_LOOP_H__
#define __SCOPED_LOOP_H__

#include <QtCore/QEventLoop>

class ScopedEventLoop
{
public:
    ScopedEventLoop(QEventLoop **proxy);
    ~ScopedEventLoop();
    void reset(QEventLoop **proxy);
    void exec();

private:
    QEventLoop m_eventLoop;
    QEventLoop **m_proxy;
};

#endif
