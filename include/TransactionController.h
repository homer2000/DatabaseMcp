#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QJsonObject>
#include <QSqlDatabase>

class TransactionController : public QObject
{
    Q_OBJECT
public:
    explicit TransactionController(QObject *parent = nullptr);

    QJsonObject manage(QSqlDatabase &db,
                       const QString &connectionName,
                       const QString &action,
                       const QString &transactionId);

    bool isInTransaction(const QString &connectionName) const;

private:
    mutable QMutex         m_mutex;
    QMap<QString, QString> m_activeTx; // connectionName -> txId
};
