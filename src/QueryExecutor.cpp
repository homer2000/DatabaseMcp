#include "QueryExecutor.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlField>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonValue>

QueryExecutor::QueryExecutor(QObject *parent)
    : QObject(parent)
{}

QueryResult QueryExecutor::execSelect(QSqlDatabase &db,
                                      const QString &sql,
                                      const QVariantList &params,
                                      int limit)
{
    QueryResult result;

    if (!db.isOpen()) {
        result.errorMessage = "Database connection is not open";
        return result;
    }

    QSqlQuery query(db);
    query.setForwardOnly(true);

    if (!query.prepare(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    for (int i = 0; i < params.size(); ++i)
        query.bindValue(i, params.at(i));

    QElapsedTimer timer;
    timer.start();

    if (!query.exec()) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    result.executionTimeMs = timer.elapsed();

    QSqlRecord rec = query.record();
    const int colCount = rec.count();
    for (int c = 0; c < colCount; ++c)
        result.columns.append(rec.fieldName(c));

    int rowsFetched = 0;
    while (query.next()) {
        if (limit > 0 && rowsFetched >= limit) break;
        QJsonObject row;
        for (int c = 0; c < colCount; ++c) {
            const QVariant v = query.value(c);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            if (v.isNull())
                row.insert(rec.fieldName(c), QJsonValue::Null);
            else
                row.insert(rec.fieldName(c), QJsonValue::fromVariant(v));
#else
            if (v.isNull())
                row.insert(rec.fieldName(c), QJsonValue());
            else
                row.insert(rec.fieldName(c), QJsonValue::fromVariant(v));
#endif
        }
        result.rows.append(row);
        ++rowsFetched;
    }

    result.rowCount = rowsFetched;
    result.success  = true;
    return result;
}

QueryResult QueryExecutor::execNonSelect(QSqlDatabase &db,
                                         const QString &sql,
                                         const QVariantList &params)
{
    QueryResult result;

    if (!db.isOpen()) {
        result.errorMessage = "Database connection is not open";
        return result;
    }

    QSqlQuery query(db);

    if (!query.prepare(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    for (int i = 0; i < params.size(); ++i)
        query.bindValue(i, params.at(i));

    QElapsedTimer timer;
    timer.start();

    if (!query.exec()) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    result.executionTimeMs = timer.elapsed();
    result.affectedRows    = query.numRowsAffected();
    result.success         = true;
    return result;
}
