#pragma once

#include <QObject>
#include <QJsonObject>
#include <QSqlDatabase>

class OptimizingEngine : public QObject
{
    Q_OBJECT
public:
    explicit OptimizingEngine(QObject *parent = nullptr);

    QJsonObject optimize(QSqlDatabase &db,
                         const QString &dbType,
                         const QString &operation,
                         const QString &table = QString());
};
