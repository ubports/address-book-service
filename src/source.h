#ifndef __GALERA_SOURCE_H__
#define __GALERA_SOURCE_H__

#include <QString>
#include <QtDBus/QtDBus>

namespace galera {

class Source
{
public:
    Source();
    Source(const Source &other);
    Source(QString id, bool isReadOnly);
    friend QDBusArgument &operator<<(QDBusArgument &argument, const Source &source);
    friend const QDBusArgument &operator>>(const QDBusArgument &argument, Source &source);

    static void registerMetaType();
    QString id() const;
    bool isReadOnly() const;
    bool isValid() const;

private:
    QString m_id;
    bool m_isReadOnly;
};

typedef QList<Source> SourceList;

QDBusArgument &operator<<(QDBusArgument &argument, const SourceList &sources);
const QDBusArgument &operator>>(const QDBusArgument &argument, SourceList &sources);

} // namespace galera

Q_DECLARE_METATYPE(galera::Source)
Q_DECLARE_METATYPE(galera::SourceList)

#endif

