#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QJsonArray>
#include <QSqlDatabase>

struct QueryResult {
    bool        success        = false;
    QString     errorMessage;
    QStringList columns;
    QJsonArray  rows;
    int         rowCount       = 0;
    qint64      executionTimeMs = 0;
    qint64      affectedRows   = 0;
};

class QueryExecutor : public QObject
{
    Q_OBJECT
public:
    explicit QueryExecutor(QObject *parent = nullptr);

    QueryResult execSelect(QSqlDatabase &db,
                           const QString &sql,
                           const QVariantList &params = {},
                           int limit = 0);

    QueryResult execNonSelect(QSqlDatabase &db,
                              const QString &sql,
                              const QVariantList &params = {});
};
