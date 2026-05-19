#pragma once

#include <QObject>
#include <QJsonObject>
#include <QSqlDatabase>

class DataExporter : public QObject
{
    Q_OBJECT
public:
    explicit DataExporter(QObject *parent = nullptr);

    QJsonObject exportData(QSqlDatabase &db,
                           const QString &source,
                           const QString &format,
                           const QString &outputPath,
                           const QVariantList &params = {});

    QJsonObject importData(QSqlDatabase &db,
                           const QString &table,
                           const QString &sourcePath,
                           const QString &format,
                           const QString &mode,
                           bool header);

    QJsonObject dumpDatabase(QSqlDatabase &db,
                             const QString &outputPath,
                             const QStringList &tables,
                             const QString &format);

    QJsonObject restoreDatabase(QSqlDatabase &db,
                                const QString &sourcePath,
                                const QString &mode);

private:
    QJsonObject exportCsv(QSqlDatabase &db, const QString &sql,
                          const QVariantList &params, const QString &path);
    QJsonObject exportJson(QSqlDatabase &db, const QString &sql,
                           const QVariantList &params, const QString &path);
    QJsonObject exportSql(QSqlDatabase &db, const QString &table,
                          const QString &path);

    QJsonObject importCsv(QSqlDatabase &db, const QString &table,
                          const QString &path, const QString &mode, bool header);
    QJsonObject importJson(QSqlDatabase &db, const QString &table,
                           const QString &path, const QString &mode);
    QJsonObject importSql(QSqlDatabase &db, const QString &path);

    static bool isSelectQuery(const QString &src);
};
