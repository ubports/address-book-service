#ifndef __GALERA_QINDIVIDUAL_H__
#define __GALERA_QINDIVIDUAL_H__

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMultiHash>
#include <QVersitProperty>

#include <folks/folks.h>

namespace galera
{

typedef QList<QtVersit::QVersitProperty> PropertyList;
class QIndividual
{
public:
    enum Field {
        All = 0x0000,
        Name = 0x0001,
        FullName = 0x0002,
        NickName = 0x0004,
        Birthday = 0x0008,
        Photo = 0x0016,
        Role = 0x0032,
        Email = 0x0064,
        Phone = 0x0128,
        Address = 0x0256,
        Im = 0x512,
        TimeZone = 0x1024,
        Url = 0x2048
    };
    Q_DECLARE_FLAGS(Fields, Field)

    QIndividual(FolksIndividual *individual);
    ~QIndividual();

    QtVersit::QVersitProperty uid() const;
    PropertyList clientPidMap() const;
    QtVersit::QVersitProperty name() const;
    QtVersit::QVersitProperty fullName() const;
    QtVersit::QVersitProperty nickName() const;
    QtVersit::QVersitProperty birthday() const;
    QtVersit::QVersitProperty photo() const;
    PropertyList roles() const;
    PropertyList emails() const;
    PropertyList phones() const;
    PropertyList addresses() const;
    PropertyList ims() const;
    QtVersit::QVersitProperty timeZone() const;
    PropertyList urls() const;
    PropertyList extra() const;

    QString toString(Fields fields = QIndividual::All) const;


private:
    FolksIndividual *m_individual;

    bool fieldsContains(Fields fields, Field value) const;
    QList<QtVersit::QVersitProperty> parseFieldList(const QString &fieldName, GeeSet *values) const;
    QMultiHash<QString, QString> parseDetails(FolksAbstractFieldDetails *details) const;
};

} //namespace
#endif
