#ifndef __GALERA_VCARD_PARSER_H__
#define __GALERA_VCARD_PARSER_H__

#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtContacts/QtContacts>

namespace galera
{

class VCardParser
{
public:

    static QStringList contactToVcard(QList<QtContacts::QContact> contacts);
    static QtContacts::QContact vcardToContact(const QString &vcard);
    static QList<QtContacts::QContact> vcardToContact(const QStringList &vcardList);
};

}

#endif
