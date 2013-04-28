#include "source.h"

namespace galera {

Source::Source()
    : m_isReadOnly(false)
{
}

Source::Source(const Source &other)
    : m_id(other.id()),
      m_isReadOnly(other.isReadOnly())
{
}

Source::Source(QString id, bool isReadOnly)
    : m_id(id),
      m_isReadOnly(isReadOnly)
{
}

bool Source::isValid() const
{
    return !m_id.isEmpty();
}

QString Source::id() const
{
    return m_id;
}

bool Source::isReadOnly() const
{
    return m_isReadOnly;
}

void Source::registerMetaType()
{
    qRegisterMetaType<Source>("Source");
    qRegisterMetaType<SourceList>("SourceList");
    qDBusRegisterMetaType<Source>();
    qDBusRegisterMetaType<SourceList>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Source &source)
{
    argument.beginStructure();
    argument << source.m_id;
    argument << source.m_isReadOnly;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Source &source)
{
    argument.beginStructure();
    argument >> source.m_id;
    argument >> source.m_isReadOnly;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SourceList &sources)
{
    argument.beginArray(qMetaTypeId<Source>());
    for(int i=0; i < sources.count(); ++i) {
        argument << sources[i];
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SourceList &sources)
{
    qDebug() << "PAser list of sources";
    argument.beginArray();
    sources.clear();
    while(!argument.atEnd()) {
        Source source;
        argument >> source;
        sources << source;
    }
    argument.endArray();
    return argument;
}

} // namespace Galera



