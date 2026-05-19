#include "SchemaInspector.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlIndex>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSqlField>

SchemaInspector::SchemaInspector(QObject *parent)
    : QObject(parent)
{}

QJsonArray SchemaInspector::listTables(QSqlDatabase &db,
                                       const QString &schema,
                                       bool includeViews)
{
    QJsonArray arr;
    if (!db.isOpen()) return arr;

    QStringList tableTypes;
    tableTypes << "Tables";
    if (includeViews) tableTypes << "Views";

    const QString drv = db.driverName();
    QStringList names;

    if (drv == "QPSQL") {
        const QString schemaName = schema.isEmpty() ? "public" : schema;
        QString sql =
            "SELECT tablename AS name, 'table' AS type "
            "FROM pg_tables WHERE schemaname = '%1' ";
        if (includeViews)
            sql += "UNION ALL SELECT viewname AS name, 'view' AS type "
                   "FROM pg_views WHERE schemaname = '%1' ";
        sql += "ORDER BY name";
        QSqlQuery q(sql.arg(schemaName), db);
        while (q.next()) {
            QJsonObject obj;
            obj["name"]   = q.value(0).toString();
            obj["type"]   = q.value(1).toString();
            obj["schema"] = schemaName;
            arr.append(obj);
        }
        return arr;
    }

    if (drv == "QMYSQL") {
        QString sql = "SHOW FULL TABLES";
        if (!db.databaseName().isEmpty())
            sql += " IN `" + db.databaseName() + "`";
        if (!includeViews) sql += " WHERE Table_type = 'BASE TABLE'";
        QSqlQuery q(sql, db);
        while (q.next()) {
            QJsonObject obj;
            obj["name"]   = q.value(0).toString();
            obj["type"]   = (q.value(1).toString() == "VIEW") ? "view" : "table";
            obj["schema"] = db.databaseName();
            arr.append(obj);
        }
        return arr;
    }

    // SQLite / ODBC — use Qt driver tables()
    const QSql::TableType tt = includeViews
        ? QSql::AllTables
        : QSql::Tables;
    for (const QString &tbl : db.tables(tt)) {
        QJsonObject obj;
        obj["name"]   = tbl;
        obj["type"]   = "table";
        obj["schema"] = schema;
        arr.append(obj);
    }
    return arr;
}

QJsonObject SchemaInspector::tableSchema(QSqlDatabase &db,
                                          const QString &tableName,
                                          const QString &schema)
{
    QJsonObject result;
    if (!db.isOpen()) {
        result["error"] = "Connection is not open";
        return result;
    }

    result["table"]  = tableName;
    result["schema"] = schema;

    const QString drv = db.driverName();
    QJsonArray columns;

    if (drv == "QPSQL") {
        const QString schemaName = schema.isEmpty() ? "public" : schema;
        const QString sql =
            "SELECT c.column_name, c.data_type, c.is_nullable, "
            "       c.column_default, "
            "       CASE WHEN pk.column_name IS NOT NULL THEN true ELSE false END AS is_pk "
            "FROM information_schema.columns c "
            "LEFT JOIN ("
            "  SELECT ku.column_name FROM information_schema.table_constraints tc "
            "  JOIN information_schema.key_column_usage ku "
            "    ON tc.constraint_name = ku.constraint_name "
            "    AND tc.table_schema = ku.table_schema "
            "  WHERE tc.constraint_type = 'PRIMARY KEY' "
            "    AND tc.table_name = :tbl AND tc.table_schema = :sch "
            ") pk ON c.column_name = pk.column_name "
            "WHERE c.table_name = :tbl2 AND c.table_schema = :sch2 "
            "ORDER BY c.ordinal_position";

        QSqlQuery q(db);
        q.prepare(sql);
        q.bindValue(":tbl",  tableName);
        q.bindValue(":sch",  schemaName);
        q.bindValue(":tbl2", tableName);
        q.bindValue(":sch2", schemaName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(0).toString();
                col["type"]        = q.value(1).toString();
                col["nullable"]    = (q.value(2).toString() == "YES");
                col["default"]     = q.value(3).isNull()
                                     ? QJsonValue()
                                     : QJsonValue(q.value(3).toString());
                col["primary_key"] = q.value(4).toBool();
                columns.append(col);
            }
        }
        result["columns"] = columns;
        return result;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.prepare("DESCRIBE `" + tableName + "`");
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(0).toString();
                col["type"]        = q.value(1).toString();
                col["nullable"]    = (q.value(2).toString() == "YES");
                col["default"]     = q.value(4).isNull()
                                     ? QJsonValue()
                                     : QJsonValue(q.value(4).toString());
                col["primary_key"] = (q.value(3).toString() == "PRI");
                columns.append(col);
            }
        }
        result["columns"] = columns;
        return result;
    }

    // SQLite
    if (drv == "QSQLITE") {
        QSqlQuery q(db);
        q.prepare("PRAGMA table_info('" + tableName + "')");
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(1).toString();
                col["type"]        = q.value(2).toString();
                col["nullable"]    = (q.value(3).toInt() == 0);
                col["default"]     = q.value(4).isNull()
                                     ? QJsonValue()
                                     : QJsonValue(q.value(4).toString());
                col["primary_key"] = (q.value(5).toInt() > 0);
                columns.append(col);
            }
        }
        result["columns"] = columns;
        return result;
    }

    // Generic fallback using QSqlRecord
    QSqlRecord rec = db.record(tableName);
    QSqlIndex  pk  = db.primaryIndex(tableName);
    for (int i = 0; i < rec.count(); ++i) {
        QSqlField f = rec.field(i);
        QJsonObject col;
        col["name"]        = f.name();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        col["type"]        = QString(QMetaType(f.metaType().id()).name());
#else
        col["type"]        = QString(QMetaType::typeName(static_cast<int>(f.type())));
#endif
        col["nullable"]    = (f.requiredStatus() != QSqlField::Required);
        col["default"]     = f.defaultValue().isNull()
                             ? QJsonValue()
                             : QJsonValue::fromVariant(f.defaultValue());
        col["primary_key"] = pk.contains(f.name());
        columns.append(col);
    }
    result["columns"] = columns;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// tableColumns — детальная информация о колонках (FK, constraints, comments)
// ─────────────────────────────────────────────────────────────────────────────

QJsonArray SchemaInspector::tableColumns(QSqlDatabase &db,
                                         const QString &tableName,
                                         const QString &schema)
{
    QJsonArray columns;
    if (!db.isOpen()) return columns;

    const QString drv = db.driverName();

    if (drv == "QPSQL") {
        const QString sch = schema.isEmpty() ? "public" : schema;
        const QString sql =
            "SELECT "
            "  c.column_name, c.data_type, c.is_nullable, c.column_default, "
            "  c.character_maximum_length, c.numeric_precision, c.numeric_scale, "
            "  CASE WHEN pk.column_name IS NOT NULL THEN true ELSE false END AS is_pk, "
            "  col_description(pgc.oid, c.ordinal_position) AS comment "
            "FROM information_schema.columns c "
            "JOIN pg_class pgc ON pgc.relname = c.table_name "
            "JOIN pg_namespace pgn ON pgn.oid = pgc.relnamespace AND pgn.nspname = :sch "
            "LEFT JOIN ( "
            "  SELECT ku.column_name FROM information_schema.table_constraints tc "
            "  JOIN information_schema.key_column_usage ku "
            "    ON tc.constraint_name = ku.constraint_name AND tc.table_schema = ku.table_schema "
            "  WHERE tc.constraint_type = 'PRIMARY KEY' AND tc.table_name = :tbl AND tc.table_schema = :sch2 "
            ") pk ON c.column_name = pk.column_name "
            "WHERE c.table_name = :tbl2 AND c.table_schema = :sch3 "
            "ORDER BY c.ordinal_position";

        QSqlQuery q(db);
        q.prepare(sql);
        q.bindValue(":tbl",  tableName); q.bindValue(":tbl2", tableName);
        q.bindValue(":sch",  sch);       q.bindValue(":sch2", sch);
        q.bindValue(":sch3", sch);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(0).toString();
                col["type"]        = q.value(1).toString();
                col["nullable"]    = (q.value(2).toString() == "YES");
                col["default"]     = q.value(3).isNull() ? QJsonValue() : QJsonValue(q.value(3).toString());
                col["max_length"]  = q.value(4).isNull() ? QJsonValue() : QJsonValue(q.value(4).toInt());
                col["precision"]   = q.value(5).isNull() ? QJsonValue() : QJsonValue(q.value(5).toInt());
                col["scale"]       = q.value(6).isNull() ? QJsonValue() : QJsonValue(q.value(6).toInt());
                col["primary_key"] = q.value(7).toBool();
                col["comment"]     = q.value(8).isNull() ? QJsonValue() : QJsonValue(q.value(8).toString());
                columns.append(col);
            }
        }
        // FK
        const QString fkSql =
            "SELECT kcu.column_name, ccu.table_name AS ref_table, ccu.column_name AS ref_col "
            "FROM information_schema.table_constraints tc "
            "JOIN information_schema.key_column_usage kcu "
            "  ON tc.constraint_name = kcu.constraint_name AND tc.table_schema = kcu.table_schema "
            "JOIN information_schema.constraint_column_usage ccu "
            "  ON ccu.constraint_name = tc.constraint_name AND ccu.table_schema = tc.table_schema "
            "WHERE tc.constraint_type = 'FOREIGN KEY' AND tc.table_name = :tbl AND tc.table_schema = :sch";
        QSqlQuery fkq(db);
        fkq.prepare(fkSql);
        fkq.bindValue(":tbl", tableName); fkq.bindValue(":sch", sch);
        QJsonObject fkMap;
        if (fkq.exec()) {
            while (fkq.next()) {
                QJsonObject fk;
                fk["ref_table"] = fkq.value(1).toString();
                fk["ref_col"]   = fkq.value(2).toString();
                fkMap[fkq.value(0).toString()] = fk;
            }
        }
        for (int i = 0; i < columns.size(); ++i) {
            QJsonObject col = columns[i].toObject();
            const QString colName = col["name"].toString();
            if (fkMap.contains(colName))
                col["foreign_key"] = fkMap[colName];
            columns[i] = col;
        }
        return columns;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.prepare(
            "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
            "  COLUMN_KEY, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, COLUMN_COMMENT "
            "FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = :tbl "
            "ORDER BY ORDINAL_POSITION");
        q.bindValue(":tbl", tableName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(0).toString();
                col["type"]        = q.value(1).toString();
                col["nullable"]    = (q.value(2).toString() == "YES");
                col["default"]     = q.value(3).isNull() ? QJsonValue() : QJsonValue(q.value(3).toString());
                col["primary_key"] = (q.value(4).toString() == "PRI");
                col["max_length"]  = q.value(5).isNull() ? QJsonValue() : QJsonValue(q.value(5).toInt());
                col["precision"]   = q.value(6).isNull() ? QJsonValue() : QJsonValue(q.value(6).toInt());
                col["scale"]       = q.value(7).isNull() ? QJsonValue() : QJsonValue(q.value(7).toInt());
                col["comment"]     = q.value(8).toString().isEmpty() ? QJsonValue() : QJsonValue(q.value(8).toString());
                columns.append(col);
            }
        }
        // FK
        QSqlQuery fkq(db);
        fkq.prepare(
            "SELECT COLUMN_NAME, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
            "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = :tbl "
            "AND REFERENCED_TABLE_NAME IS NOT NULL");
        fkq.bindValue(":tbl", tableName);
        QJsonObject fkMap;
        if (fkq.exec()) {
            while (fkq.next()) {
                QJsonObject fk;
                fk["ref_table"] = fkq.value(1).toString();
                fk["ref_col"]   = fkq.value(2).toString();
                fkMap[fkq.value(0).toString()] = fk;
            }
        }
        for (int i = 0; i < columns.size(); ++i) {
            QJsonObject col = columns[i].toObject();
            if (fkMap.contains(col["name"].toString()))
                col["foreign_key"] = fkMap[col["name"].toString()];
            columns[i] = col;
        }
        return columns;
    }

    // SQLite
    if (drv == "QSQLITE") {
        QSqlQuery q(db);
        q.prepare(QString("PRAGMA table_info('%1')").arg(tableName));
        if (q.exec()) {
            while (q.next()) {
                QJsonObject col;
                col["name"]        = q.value(1).toString();
                col["type"]        = q.value(2).toString();
                col["nullable"]    = (q.value(3).toInt() == 0);
                col["default"]     = q.value(4).isNull() ? QJsonValue() : QJsonValue(q.value(4).toString());
                col["primary_key"] = (q.value(5).toInt() > 0);
                columns.append(col);
            }
        }
        // FK
        QSqlQuery fkq(db);
        fkq.prepare(QString("PRAGMA foreign_key_list('%1')").arg(tableName));
        QJsonObject fkMap;
        if (fkq.exec()) {
            while (fkq.next()) {
                QJsonObject fk;
                fk["ref_table"] = fkq.value(2).toString();
                fk["ref_col"]   = fkq.value(4).toString();
                fkMap[fkq.value(3).toString()] = fk;
            }
        }
        for (int i = 0; i < columns.size(); ++i) {
            QJsonObject col = columns[i].toObject();
            if (fkMap.contains(col["name"].toString()))
                col["foreign_key"] = fkMap[col["name"].toString()];
            columns[i] = col;
        }
    }
    return columns;
}

// ─────────────────────────────────────────────────────────────────────────────
// tableIndices
// ─────────────────────────────────────────────────────────────────────────────

QJsonArray SchemaInspector::tableIndices(QSqlDatabase &db, const QString &tableName)
{
    QJsonArray result;
    if (!db.isOpen()) return result;
    const QString drv = db.driverName();

    if (drv == "QPSQL") {
        QSqlQuery q(db);
        q.prepare(
            "SELECT i.relname AS index_name, ix.indisunique, ix.indisprimary, "
            "  array_to_string(ARRAY(SELECT a.attname FROM pg_attribute a "
            "    WHERE a.attrelid = t.oid AND a.attnum = ANY(ix.indkey)), ',') AS columns, "
            "  am.amname AS index_type "
            "FROM pg_class t "
            "JOIN pg_index ix ON t.oid = ix.indrelid "
            "JOIN pg_class i  ON i.oid  = ix.indexrelid "
            "JOIN pg_am    am ON am.oid = i.relam "
            "WHERE t.relname = :tbl AND t.relkind = 'r'");
        q.bindValue(":tbl", tableName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject idx;
                idx["name"]    = q.value(0).toString();
                idx["unique"]  = q.value(1).toBool();
                idx["primary"] = q.value(2).toBool();
                idx["columns"] = QJsonArray::fromStringList(q.value(3).toString().split(','));
                idx["type"]    = q.value(4).toString().toUpper();
                result.append(idx);
            }
        }
        return result;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.prepare("SHOW INDEX FROM `" + tableName + "`");
        QMap<QString, QJsonObject> idxMap;
        if (q.exec()) {
            while (q.next()) {
                const QString idxName = q.value(2).toString();
                if (!idxMap.contains(idxName)) {
                    QJsonObject idx;
                    idx["name"]    = idxName;
                    idx["unique"]  = (q.value(1).toInt() == 0);
                    idx["primary"] = (idxName == "PRIMARY");
                    idx["type"]    = q.value(10).toString();
                    idx["columns"] = QJsonArray();
                    idxMap[idxName] = idx;
                }
                QJsonObject &idx = idxMap[idxName];
                QJsonArray cols = idx["columns"].toArray();
                cols.append(q.value(4).toString());
                idx["columns"] = cols;
            }
        }
        for (const QJsonObject &idx : idxMap.values()) result.append(idx);
        return result;
    }

    if (drv == "QSQLITE") {
        QSqlQuery q(db);
        q.prepare(QString("PRAGMA index_list('%1')").arg(tableName));
        if (q.exec()) {
            while (q.next()) {
                const QString idxName = q.value(1).toString();
                QJsonObject idx;
                idx["name"]    = idxName;
                idx["unique"]  = (q.value(2).toInt() == 1);
                idx["primary"] = idxName.startsWith("sqlite_autoindex_");
                idx["type"]    = "BTREE";
                QJsonArray cols;
                QSqlQuery ci(db);
                ci.prepare(QString("PRAGMA index_info('%1')").arg(idxName));
                if (ci.exec()) while (ci.next()) cols.append(ci.value(2).toString());
                idx["columns"] = cols;
                result.append(idx);
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// tableTriggers
// ─────────────────────────────────────────────────────────────────────────────

QJsonArray SchemaInspector::tableTriggers(QSqlDatabase &db, const QString &tableName)
{
    QJsonArray result;
    if (!db.isOpen()) return result;
    const QString drv = db.driverName();

    if (drv == "QPSQL") {
        QSqlQuery q(db);
        q.prepare(
            "SELECT trigger_name, event_manipulation, action_timing, "
            "       action_orientation, action_statement "
            "FROM information_schema.triggers "
            "WHERE event_object_table = :tbl "
            "ORDER BY trigger_name");
        q.bindValue(":tbl", tableName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject t;
                t["name"]        = q.value(0).toString();
                t["event"]       = q.value(1).toString();
                t["timing"]      = q.value(2).toString();
                t["orientation"] = q.value(3).toString();
                t["body"]        = q.value(4).toString();
                result.append(t);
            }
        }
        return result;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.prepare(
            "SELECT TRIGGER_NAME, EVENT_MANIPULATION, ACTION_TIMING, "
            "       ACTION_ORIENTATION, ACTION_STATEMENT "
            "FROM INFORMATION_SCHEMA.TRIGGERS "
            "WHERE EVENT_OBJECT_TABLE = :tbl AND TRIGGER_SCHEMA = DATABASE() "
            "ORDER BY TRIGGER_NAME");
        q.bindValue(":tbl", tableName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject t;
                t["name"]        = q.value(0).toString();
                t["event"]       = q.value(1).toString();
                t["timing"]      = q.value(2).toString();
                t["orientation"] = q.value(3).toString();
                t["body"]        = q.value(4).toString();
                result.append(t);
            }
        }
        return result;
    }

    if (drv == "QSQLITE") {
        QSqlQuery q(db);
        q.prepare(
            "SELECT name, sql FROM sqlite_master "
            "WHERE type = 'trigger' AND tbl_name = :tbl "
            "ORDER BY name");
        q.bindValue(":tbl", tableName);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject t;
                t["name"] = q.value(0).toString();
                t["body"] = q.value(1).toString();
                result.append(t);
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// listFunctions
// ─────────────────────────────────────────────────────────────────────────────

QJsonArray SchemaInspector::listFunctions(QSqlDatabase &db)
{
    QJsonArray result;
    if (!db.isOpen()) return result;
    const QString drv = db.driverName();

    if (drv == "QPSQL") {
        QSqlQuery q(db);
        q.exec(
            "SELECT routine_name, routine_type, data_type, routine_definition "
            "FROM information_schema.routines "
            "WHERE routine_schema NOT IN ('pg_catalog','information_schema') "
            "ORDER BY routine_name");
        while (q.next()) {
            QJsonObject f;
            f["name"]       = q.value(0).toString();
            f["type"]       = q.value(1).toString();
            f["return_type"]= q.value(2).toString();
            f["body"]       = q.value(3).toString();
            result.append(f);
        }
        return result;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.exec(
            "SELECT ROUTINE_NAME, ROUTINE_TYPE, DTD_IDENTIFIER, ROUTINE_DEFINITION "
            "FROM INFORMATION_SCHEMA.ROUTINES "
            "WHERE ROUTINE_SCHEMA = DATABASE() "
            "ORDER BY ROUTINE_NAME");
        while (q.next()) {
            QJsonObject f;
            f["name"]        = q.value(0).toString();
            f["type"]        = q.value(1).toString();
            f["return_type"] = q.value(2).toString();
            f["body"]        = q.value(3).toString();
            result.append(f);
        }
        return result;
    }

    if (drv == "QSQLITE") {
        // SQLite не поддерживает хранимые функции — возвращаем пустой массив
        // с пояснением
        QJsonObject note;
        note["info"] = "SQLite does not support stored functions or procedures";
        result.append(note);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// dbSize
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject SchemaInspector::dbSize(QSqlDatabase &db)
{
    QJsonObject result;
    if (!db.isOpen()) {
        result["error"] = "Connection is not open";
        return result;
    }
    const QString drv = db.driverName();

    if (drv == "QPSQL") {
        QSqlQuery q(db);
        q.exec("SELECT pg_database_size(current_database())");
        if (q.next()) result["total_bytes"] = q.value(0).toLongLong();

        QJsonArray tables;
        q.exec(
            "SELECT relname, "
            "  pg_total_relation_size(oid) AS total, "
            "  pg_relation_size(oid) AS data, "
            "  pg_indexes_size(oid) AS indexes, "
            "  reltuples::bigint AS est_rows "
            "FROM pg_class "
            "WHERE relkind = 'r' AND relnamespace NOT IN "
            "  (SELECT oid FROM pg_namespace WHERE nspname IN ('pg_catalog','information_schema')) "
            "ORDER BY total DESC");
        while (q.next()) {
            QJsonObject t;
            t["table"]        = q.value(0).toString();
            t["total_bytes"]  = q.value(1).toLongLong();
            t["data_bytes"]   = q.value(2).toLongLong();
            t["index_bytes"]  = q.value(3).toLongLong();
            t["est_rows"]     = q.value(4).toLongLong();
            tables.append(t);
        }
        result["tables"] = tables;
        return result;
    }

    if (drv == "QMYSQL") {
        QSqlQuery q(db);
        q.exec(
            "SELECT TABLE_NAME, DATA_LENGTH, INDEX_LENGTH, "
            "  DATA_LENGTH + INDEX_LENGTH AS TOTAL_LENGTH, TABLE_ROWS, AVG_ROW_LENGTH "
            "FROM INFORMATION_SCHEMA.TABLES "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_TYPE = 'BASE TABLE' "
            "ORDER BY TOTAL_LENGTH DESC");
        qint64 totalBytes = 0;
        QJsonArray tables;
        while (q.next()) {
            QJsonObject t;
            t["table"]        = q.value(0).toString();
            t["data_bytes"]   = q.value(1).toLongLong();
            t["index_bytes"]  = q.value(2).toLongLong();
            t["total_bytes"]  = q.value(3).toLongLong();
            t["est_rows"]     = q.value(4).toLongLong();
            t["avg_row_len"]  = q.value(5).toLongLong();
            totalBytes += q.value(3).toLongLong();
            tables.append(t);
        }
        result["total_bytes"] = totalBytes;
        result["tables"]      = tables;
        return result;
    }

    if (drv == "QSQLITE") {
        QSqlQuery q(db);
        q.exec("PRAGMA page_count");
        qint64 pageCount = q.next() ? q.value(0).toLongLong() : 0;
        q.exec("PRAGMA page_size");
        qint64 pageSize  = q.next() ? q.value(0).toLongLong() : 4096;
        result["total_bytes"] = pageCount * pageSize;

        QJsonArray tables;
        q.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
        QStringList tableNames;
        while (q.next()) tableNames << q.value(0).toString();
        for (const QString &tbl : tableNames) {
            QSqlQuery cq(db);
            cq.exec(QString("SELECT COUNT(*) FROM '%1'").arg(tbl));
            QJsonObject t;
            t["table"]    = tbl;
            t["est_rows"] = cq.next() ? cq.value(0).toLongLong() : 0;
            tables.append(t);
        }
        result["tables"] = tables;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// explainQuery
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject SchemaInspector::explainQuery(QSqlDatabase &db,
                                           const QString &sql,
                                           bool analyze)
{
    QJsonObject result;
    if (!db.isOpen()) {
        result["error"] = "Connection is not open";
        return result;
    }
    const QString drv = db.driverName();

    QString explainSql;
    if (drv == "QPSQL") {
        explainSql = analyze
            ? QString("EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT) %1").arg(sql)
            : QString("EXPLAIN (FORMAT TEXT) %1").arg(sql);
    } else if (drv == "QMYSQL") {
        explainSql = analyze
            ? QString("EXPLAIN ANALYZE %1").arg(sql)
            : QString("EXPLAIN %1").arg(sql);
    } else if (drv == "QSQLITE") {
        explainSql = QString("EXPLAIN QUERY PLAN %1").arg(sql);
    } else {
        result["error"] = "EXPLAIN not supported for this driver";
        return result;
    }

    QSqlQuery q(db);
    QJsonArray plan;

    if (!q.exec(explainSql)) {
        result["error"] = q.lastError().text();
        return result;
    }

    if (drv == "QPSQL") {
        QStringList lines;
        while (q.next()) lines << q.value(0).toString();
        result["plan_text"] = lines.join('\n');
        result["success"]   = true;
        return result;
    }

    QSqlRecord rec = q.record();
    while (q.next()) {
        QJsonObject row;
        for (int c = 0; c < rec.count(); ++c)
            row[rec.fieldName(c)] = QJsonValue::fromVariant(q.value(c));
        plan.append(row);
    }
    result["plan"]    = plan;
    result["success"] = true;
    return result;
}
