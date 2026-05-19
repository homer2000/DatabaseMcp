#include "TransactionController.h"

#include <QMutexLocker>
#include <QDateTime>
#include <QSqlError>

TransactionController::TransactionController(QObject *parent)
    : QObject(parent)
{}

QJsonObject TransactionController::manage(QSqlDatabase &db,
                                          const QString &connectionName,
                                          const QString &action,
                                          const QString &transactionId)
{
    QJsonObject res;
    const QString act = action.toLower();

    if (act == "begin") {
        if (!db.isOpen()) {
            res["success"] = false;
            res["error"]   = "Connection is not open";
            return res;
        }
        if (!db.transaction()) {
            res["success"] = false;
            res["error"]   = db.lastError().text();
            return res;
        }
        const QString txId = QString("tx_%1").arg(
            QDateTime::currentMSecsSinceEpoch());
        {
            QMutexLocker lock(&m_mutex);
            m_activeTx.insert(connectionName, txId);
        }
        res["success"]        = true;
        res["transaction_id"] = txId;
        return res;
    }

    if (act == "commit") {
        if (!db.commit()) {
            res["success"] = false;
            res["error"]   = db.lastError().text();
            return res;
        }
        { QMutexLocker lock(&m_mutex); m_activeTx.remove(connectionName); }
        res["success"] = true;
        res["message"] = "Transaction committed";
        return res;
    }

    if (act == "rollback") {
        if (!transactionId.isEmpty()) {
            QMutexLocker lock(&m_mutex);
            if (!m_activeTx.contains(connectionName) ||
                m_activeTx.value(connectionName) != transactionId) {
                res["success"] = false;
                res["error"]   = "Transaction not found: " + transactionId;
                return res;
            }
        }
        if (!db.rollback()) {
            res["success"] = false;
            res["error"]   = db.lastError().text();
            return res;
        }
        { QMutexLocker lock(&m_mutex); m_activeTx.remove(connectionName); }
        res["success"] = true;
        res["message"] = "Transaction rolled back";
        return res;
    }

    res["success"] = false;
    res["error"]   = QString("Unknown action: %1").arg(action);
    return res;
}

bool TransactionController::isInTransaction(const QString &connectionName) const
{
    QMutexLocker lock(&m_mutex);
    return m_activeTx.contains(connectionName);
}
