#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>

class SchemaInspector : public QObject
{
    Q_OBJECT
public:
    explicit SchemaInspector(QObject *parent = nullptr);

    QJsonArray  listTables(QSqlDatabase &db,
                           const QString &schema,
                           bool includeViews);

    QJsonObject tableSchema(QSqlDatabase &db,
                            const QString &tableName,
                            const QString &schema);

    QJsonArray  tableColumns(QSqlDatabase &db,
                             const QString &tableName,
                             const QString &schema);

    QJsonArray  tableIndices(QSqlDatabase &db,
                             const QString &tableName);

    QJsonArray  tableTriggers(QSqlDatabase &db,
                              const QString &tableName);

    QJsonArray  listFunctions(QSqlDatabase &db);

    QJsonObject dbSize(QSqlDatabase &db);

    QJsonObject explainQuery(QSqlDatabase &db,
                             const QString &sql,
                             bool analyze);
};
