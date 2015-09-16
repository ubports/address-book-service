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

#include "ab-update-adaptor.h"

ABUpdateAdaptor::ABUpdateAdaptor(ABUpdate *parent)
    : QDBusAbstractAdaptor(parent),
      m_abUpdate(parent)
{
    setAutoRelaySignals(true);
}

ABUpdateAdaptor::~ABUpdateAdaptor()
{
}

bool ABUpdateAdaptor::needsUpdate() const
{
    return !m_abUpdate->needsUpdate().isEmpty();
}

bool ABUpdateAdaptor::isRunning() const
{
    return m_abUpdate->isRunning();
}

void ABUpdateAdaptor::startUpdate()
{
    m_abUpdate->startUpdate();
}

void ABUpdateAdaptor::cancelUpdate()
{
    m_abUpdate->cancelUpdate();
}
