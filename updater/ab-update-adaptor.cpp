#include "ab-update-adaptor.h"


ABUpdateAdaptor::ABUpdateAdaptor(ABUpdate *parent)
    : QDBusAbstractAdaptor(parent),
      m_abUpdate(parent)
{
    setAutoRelaySignals(true);
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
