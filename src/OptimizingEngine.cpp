#include "OptimizingEngine.h"

#include <QSqlQuery>
#include <QSqlError>

OptimizingEngine::OptimizingEngine(QObject *parent)
    : QObject(parent)
{}

QJsonObject OptimizingEngine::optimize(QSqlDatabase &db,
                                        const QString &dbType,
                                        const QString &operation,
                                        const QString &table)
{
    QJsonObject res;
    if (!db.isOpen()) {
        res["success"] = false;
        res["error"]   = "Connection is not open";
        return res;
    }

    const QString op  = operation.toLower();
    const QString drv = db.driverName();
    QString sql;

    if (drv == "QPSQL") {
        if (op == "vacuum") {
            sql = table.isEmpty()
                ? "VACUUM ANALYZE"
                : QString("VACUUM ANALYZE %1").arg(table);
        } else if (op == "analyze") {
            sql = table.isEmpty()
                ? "ANALYZE"
                : QString("ANALYZE %1").arg(table);
        } else if (op == "reindex") {
            sql = table.isEmpty()
                ? "REINDEX DATABASE CURRENT"
                : QString("REINDEX TABLE %1").arg(table);
        } else {
            res["success"] = false;
            res["error"]   = QString("Unknown operation: %1").arg(operation);
            return res;
        }
    } else if (drv == "QMYSQL") {
        const QString tbl = table.isEmpty() ? "*" : table;
        if (op == "analyze") {
            sql = table.isEmpty() ? QString() : QString("ANALYZE TABLE %1").arg(tbl);
        } else if (op == "vacuum" || op == "optimize") {
            sql = table.isEmpty() ? QString() : QString("OPTIMIZE TABLE %1").arg(tbl);
        } else if (op == "reindex" || op == "repair") {
            sql = table.isEmpty() ? QString() : QString("REPAIR TABLE %1").arg(tbl);
        } else {
            res["success"] = false;
            res["error"]   = QString("Unknown operation: %1").arg(operation);
            return res;
        }
        if (sql.isEmpty()) {
            res["success"] = false;
            res["error"]   = "Table name required for MySQL optimize operations";
            return res;
        }
    } else if (drv == "QSQLITE") {
        if (op == "vacuum") {
            sql = "VACUUM";
        } else if (op == "analyze") {
            sql = "ANALYZE";
        } else if (op == "reindex") {
            sql = table.isEmpty() ? "REINDEX" : QString("REINDEX %1").arg(table);
        } else {
            res["success"] = false;
            res["error"]   = QString("Unknown operation: %1").arg(operation);
            return res;
        }
    } else {
        res["success"] = false;
        res["error"]   = QString("Optimize not supported for driver: %1").arg(drv);
        return res;
    }

    QSqlQuery q(db);
    if (!q.exec(sql)) {
        res["success"] = false;
        res["error"]   = q.lastError().text();
        return res;
    }

    res["success"]   = true;
    res["operation"] = operation;
    if (!table.isEmpty()) res["table"] = table;
    res["sql"] = sql;
    return res;
}
