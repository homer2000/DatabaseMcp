#include "DataExporter.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

DataExporter::DataExporter(QObject *parent)
    : QObject(parent)
{}

bool DataExporter::isSelectQuery(const QString &src)
{
    return src.trimmed().startsWith("SELECT", Qt::CaseInsensitive) ||
           src.trimmed().startsWith("WITH",   Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────────────────────────────────────
// Export
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject DataExporter::exportData(QSqlDatabase &db,
                                     const QString &source,
                                     const QString &format,
                                     const QString &outputPath,
                                     const QVariantList &params)
{
    const QString sql = isSelectQuery(source)
        ? source
        : QString("SELECT * FROM %1").arg(source);

    const QString fmt = format.toLower();
    if (fmt == "csv")  return exportCsv(db, sql, params, outputPath);
    if (fmt == "json") return exportJson(db, sql, params, outputPath);
    if (fmt == "sql")  return exportSql(db, source, outputPath);

    QJsonObject err;
    err["success"] = false;
    err["error"]   = QString("Unsupported export format: %1").arg(format);
    return err;
}

QJsonObject DataExporter::exportCsv(QSqlDatabase &db,
                                    const QString &sql,
                                    const QVariantList &params,
                                    const QString &path)
{
    QJsonObject res;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif

    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(sql);
    for (int i = 0; i < params.size(); ++i) q.bindValue(i, params.at(i));

    if (!q.exec()) {
        res["success"] = false;
        res["error"]   = q.lastError().text();
        return res;
    }

    QSqlRecord rec = q.record();
    const int cols = rec.count();

    // Header
    QStringList header;
    for (int c = 0; c < cols; ++c) header << rec.fieldName(c);
    out << header.join(',') << '\n';

    int rows = 0;
    while (q.next()) {
        QStringList row;
        for (int c = 0; c < cols; ++c) {
            QString v = q.value(c).toString();
            if (v.contains(',') || v.contains('"') || v.contains('\n'))
                v = '"' + QString(v).replace('"', "\"\"") + '"';
            row << v;
        }
        out << row.join(',') << '\n';
        ++rows;
    }

    file.close();
    res["success"]       = true;
    res["file_path"]     = path;
    res["rows_exported"] = rows;
    res["file_size_bytes"] = QFileInfo(path).size();
    return res;
}

QJsonObject DataExporter::exportJson(QSqlDatabase &db,
                                     const QString &sql,
                                     const QVariantList &params,
                                     const QString &path)
{
    QJsonObject res;
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(sql);
    for (int i = 0; i < params.size(); ++i) q.bindValue(i, params.at(i));

    if (!q.exec()) {
        res["success"] = false;
        res["error"]   = q.lastError().text();
        return res;
    }

    QSqlRecord rec = q.record();
    const int cols = rec.count();
    QJsonArray arr;

    while (q.next()) {
        QJsonObject row;
        for (int c = 0; c < cols; ++c) {
            const QVariant v = q.value(c);
            row[rec.fieldName(c)] = v.isNull()
                ? QJsonValue()
                : QJsonValue::fromVariant(v);
        }
        arr.append(row);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();

    res["success"]         = true;
    res["file_path"]       = path;
    res["rows_exported"]   = arr.size();
    res["file_size_bytes"] = QFileInfo(path).size();
    return res;
}

QJsonObject DataExporter::exportSql(QSqlDatabase &db,
                                    const QString &table,
                                    const QString &path)
{
    QJsonObject res;

    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.exec(QString("SELECT * FROM %1").arg(table))) {
        res["success"] = false;
        res["error"]   = q.lastError().text();
        return res;
    }

    QSqlRecord rec = q.record();
    const int cols = rec.count();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif

    int rows = 0;
    while (q.next()) {
        QStringList vals;
        for (int c = 0; c < cols; ++c) {
            const QVariant v = q.value(c);
            if (v.isNull()) {
                vals << "NULL";
            } else {
                const QString s = v.toString();
                vals << ("'" + QString(s).replace('\'', "''") + "'");
            }
        }
        out << "INSERT INTO " << table << " VALUES (" << vals.join(", ") << ");\n";
        ++rows;
    }

    file.close();
    res["success"]         = true;
    res["file_path"]       = path;
    res["rows_exported"]   = rows;
    res["file_size_bytes"] = QFileInfo(path).size();
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// Import
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject DataExporter::importData(QSqlDatabase &db,
                                     const QString &table,
                                     const QString &sourcePath,
                                     const QString &format,
                                     const QString &mode,
                                     bool header)
{
    QString fmt = format.toLower();
    if (fmt.isEmpty()) {
        const QString ext = QFileInfo(sourcePath).suffix().toLower();
        if (ext == "csv") fmt = "csv";
        else if (ext == "json") fmt = "json";
        else if (ext == "sql") fmt = "sql";
        else fmt = "csv";
    }

    if (fmt == "csv")  return importCsv(db, table, sourcePath, mode, header);
    if (fmt == "json") return importJson(db, table, sourcePath, mode);
    if (fmt == "sql")  return importSql(db, sourcePath);

    QJsonObject err;
    err["success"] = false;
    err["error"]   = QString("Unsupported import format: %1").arg(fmt);
    return err;
}

QJsonObject DataExporter::importCsv(QSqlDatabase &db,
                                    const QString &table,
                                    const QString &path,
                                    const QString &mode,
                                    bool header)
{
    QJsonObject res;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }

    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#else
    in.setEncoding(QStringConverter::Utf8);
#endif

    QStringList columns;
    int imported = 0;
    QJsonArray errors;

    if (mode.toLower() == "replace") {
        QSqlQuery del(db);
        del.exec(QString("DELETE FROM %1").arg(table));
    }

    bool firstLine = true;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Simple CSV split (no multi-line field support)
        QStringList fields;
        QString cur;
        bool inQuote = false;
        for (int i = 0; i < line.size(); ++i) {
            QChar ch = line[i];
            if (ch == '"') {
                if (inQuote && i + 1 < line.size() && line[i+1] == '"') {
                    cur += '"'; ++i;
                } else {
                    inQuote = !inQuote;
                }
            } else if (ch == ',' && !inQuote) {
                fields << cur; cur.clear();
            } else {
                cur += ch;
            }
        }
        fields << cur;

        if (firstLine) {
            firstLine = false;
            if (header) { columns = fields; continue; }
            else {
                for (int i = 0; i < fields.size(); ++i)
                    columns << QString("col%1").arg(i);
            }
        }

        if (fields.size() != columns.size()) {
            errors.append(QString("Row %1: column count mismatch").arg(imported + 1));
            continue;
        }

        QStringList placeholders;
        for (int i = 0; i < columns.size(); ++i)
            placeholders << "?";

        const QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
            .arg(table)
            .arg(columns.join(", "))
            .arg(placeholders.join(", "));

        QSqlQuery q(db);
        q.prepare(sql);
        for (int i = 0; i < fields.size(); ++i)
            q.bindValue(i, fields.at(i));

        if (q.exec()) {
            ++imported;
        } else {
            errors.append(q.lastError().text());
        }
    }

    file.close();
    res["success"]       = true;
    res["imported_rows"] = imported;
    res["errors"]        = errors;
    return res;
}

QJsonObject DataExporter::importJson(QSqlDatabase &db,
                                     const QString &table,
                                     const QString &path,
                                     const QString &mode)
{
    QJsonObject res;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &pe);
    file.close();

    if (pe.error != QJsonParseError::NoError) {
        res["success"] = false;
        res["error"]   = pe.errorString();
        return res;
    }

    QJsonArray records = doc.isArray() ? doc.array() : QJsonArray { doc.object() };

    if (mode.toLower() == "replace") {
        QSqlQuery del(db);
        del.exec(QString("DELETE FROM %1").arg(table));
    }

    int imported = 0;
    QJsonArray errors;

    for (const QJsonValue &val : records) {
        if (!val.isObject()) { errors.append("Non-object record skipped"); continue; }
        const QJsonObject rec = val.toObject();
        const QStringList cols = rec.keys();

        QStringList placeholders;
        for (int i = 0; i < cols.size(); ++i) placeholders << "?";

        QSqlQuery q(db);
        q.prepare(QString("INSERT INTO %1 (%2) VALUES (%3)")
                  .arg(table)
                  .arg(cols.join(", "))
                  .arg(placeholders.join(", ")));

        for (int i = 0; i < cols.size(); ++i)
            q.bindValue(i, rec.value(cols.at(i)).toVariant());

        if (q.exec()) ++imported;
        else errors.append(q.lastError().text());
    }

    res["success"]       = true;
    res["imported_rows"] = imported;
    res["errors"]        = errors;
    return res;
}

QJsonObject DataExporter::importSql(QSqlDatabase &db, const QString &path)
{
    QJsonObject res;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        res["success"] = false;
        res["error"]   = file.errorString();
        return res;
    }

    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#else
    in.setEncoding(QStringConverter::Utf8);
#endif

    int executed = 0;
    QJsonArray errors;
    QString stmt;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("--")) continue;
        stmt += ' ' + line;
        if (stmt.trimmed().endsWith(';')) {
            stmt = stmt.trimmed();
            stmt.chop(1);
            QSqlQuery q(db);
            if (q.exec(stmt)) ++executed;
            else errors.append(q.lastError().text());
            stmt.clear();
        }
    }
    file.close();

    res["success"]       = true;
    res["imported_rows"] = executed;
    res["errors"]        = errors;
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// dumpDatabase
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject DataExporter::dumpDatabase(QSqlDatabase &db,
                                        const QString &outputPath,
                                        const QStringList &tables,
                                        const QString &format)
{
    QJsonObject res;
    if (!db.isOpen()) {
        res["success"] = false; res["error"] = "Connection is not open";
        return res;
    }

    // Определяем список таблиц
    QStringList tblList = tables;
    if (tblList.isEmpty()) {
        const QSqlDatabase &cdb = db;
        tblList = const_cast<QSqlDatabase&>(cdb).tables(QSql::Tables);
    }

    const QString fmt = format.toLower();
    if (fmt == "json") {
        QJsonObject dump;
        for (const QString &tbl : tblList) {
            QSqlQuery q(db);
            q.setForwardOnly(true);
            if (!q.exec(QString("SELECT * FROM %1").arg(tbl))) continue;
            QSqlRecord rec = q.record();
            const int cols = rec.count();
            QJsonArray rows;
            while (q.next()) {
                QJsonObject row;
                for (int c = 0; c < cols; ++c) {
                    const QVariant v = q.value(c);
                    row[rec.fieldName(c)] = v.isNull() ? QJsonValue() : QJsonValue::fromVariant(v);
                }
                rows.append(row);
            }
            dump[tbl] = rows;
        }
        QFile f(outputPath);
        if (!f.open(QIODevice::WriteOnly)) {
            res["success"] = false; res["error"] = f.errorString();
            return res;
        }
        f.write(QJsonDocument(dump).toJson(QJsonDocument::Indented));
        f.close();
        res["success"]      = true;
        res["tables_dumped"]= (int)tblList.size();
        res["file_path"]    = outputPath;
        res["file_size_bytes"] = QFileInfo(outputPath).size();
        return res;
    }

    // SQL format (default)
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        res["success"] = false; res["error"] = file.errorString();
        return res;
    }
    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif

    const QString drv = db.driverName();
    int totalRows = 0;

    for (const QString &tbl : tblList) {
        // DDL — для SQLite берём из sqlite_master
        if (drv == "QSQLITE") {
            QSqlQuery ddl(db);
            ddl.prepare("SELECT sql FROM sqlite_master WHERE type='table' AND name=:t");
            ddl.bindValue(":t", tbl);
            if (ddl.exec() && ddl.next())
                out << ddl.value(0).toString() << ";\n\n";
        } else {
            out << "-- Table: " << tbl << "\n";
        }

        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QString("SELECT * FROM %1").arg(tbl))) continue;
        QSqlRecord rec = q.record();
        const int cols = rec.count();
        QStringList colNames;
        for (int c = 0; c < cols; ++c) colNames << rec.fieldName(c);

        while (q.next()) {
            QStringList vals;
            for (int c = 0; c < cols; ++c) {
                const QVariant v = q.value(c);
                if (v.isNull()) vals << "NULL";
                else vals << ("'" + QString(v.toString()).replace('\'', "''") + "'");
            }
            out << "INSERT INTO " << tbl
                << " (" << colNames.join(", ") << ")"
                << " VALUES (" << vals.join(", ") << ");\n";
            ++totalRows;
        }
        out << "\n";
    }
    file.close();

    res["success"]         = true;
    res["tables_dumped"]   = (int)tblList.size();
    res["rows_dumped"]     = totalRows;
    res["file_path"]       = outputPath;
    res["file_size_bytes"] = QFileInfo(outputPath).size();
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// restoreDatabase
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject DataExporter::restoreDatabase(QSqlDatabase &db,
                                           const QString &sourcePath,
                                           const QString &mode)
{
    const QString ext = QFileInfo(sourcePath).suffix().toLower();

    if (ext == "json") {
        QFile f(sourcePath);
        if (!f.open(QIODevice::ReadOnly)) {
            QJsonObject e; e["success"] = false; e["error"] = f.errorString(); return e;
        }
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
        f.close();
        if (pe.error != QJsonParseError::NoError) {
            QJsonObject e; e["success"] = false; e["error"] = pe.errorString(); return e;
        }
        const QJsonObject dump = doc.object();
        int imported = 0;
        QJsonArray errors;
        for (const QString &tbl : dump.keys()) {
            if (mode.toLower() == "replace") {
                QSqlQuery del(db);
                del.exec(QString("DELETE FROM %1").arg(tbl));
            }
            const QJsonArray rows = dump[tbl].toArray();
            for (const QJsonValue &val : rows) {
                if (!val.isObject()) continue;
                const QJsonObject row = val.toObject();
                const QStringList cols = row.keys();
                QStringList ph; for (int i = 0; i < cols.size(); ++i) ph << "?";
                QSqlQuery q(db);
                q.prepare(QString("INSERT INTO %1 (%2) VALUES (%3)")
                          .arg(tbl, cols.join(", "), ph.join(", ")));
                for (int i = 0; i < cols.size(); ++i)
                    q.bindValue(i, row.value(cols[i]).toVariant());
                if (q.exec()) ++imported;
                else errors.append(q.lastError().text());
            }
        }
        QJsonObject res;
        res["success"]       = true;
        res["imported_rows"] = imported;
        res["errors"]        = errors;
        return res;
    }

    // SQL файл
    return importSql(db, sourcePath);
}
