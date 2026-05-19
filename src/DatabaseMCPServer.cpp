#include "DatabaseMCPServer.h"
#include "AccountsConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

DatabaseMCPServer::DatabaseMCPServer(const QString &configDir, QObject *parent)
    : QMCPServer("DatabaseMCPServer", "1.0.0", parent)
    , m_configDir(configDir)
    , m_accounts(this)
    , m_connMgr(this)
    , m_executor(this)
    , m_inspector(this)
    , m_exporter(this)
    , m_txCtrl(this)
    , m_optimizer(this)
{
    setInstructions(
        "Database MCP Server. Provides tools to connect and work with "
        "PostgreSQL, MySQL, SQLite and MSSQL databases via Qt SQL drivers. "
        "Use db_list_accounts to see available pre-configured accounts, "
        "then db_connect_account to open a connection by account name.");

    QString errMsg;
    if (!m_accounts.load(m_configDir, errMsg)) {
        dbg(QString("[DatabaseMCPServer] WARNING: %1").arg(errMsg));
    }
}

void DatabaseMCPServer::onInitialized()
{
    // db_connect
    registerTool(MCPToolDefinition {
        "db_connect",
        "Connect to a database",
        {
            { "connection_name", "string",  "Unique connection identifier", true  },
            { "db_type",         "string",  "postgres/mysql/sqlite/mssql",  true  },
            { "host",            "string",  "Host (default localhost)",      false },
            { "port",            "integer", "Port number",                   false },
            { "database",        "string",  "Database name or file path",    true  },
            { "username",        "string",  "Username",                      false },
            { "password",        "string",  "Password",                      false },
            { "ssl_mode",        "string",  "disable/request/require",       false }
        },
        "object"
    });

    // db_query
    registerTool(MCPToolDefinition {
        "db_query",
        "Execute a SELECT query",
        {
            { "connection_id", "string",  "Connection identifier",    true  },
            { "sql",           "string",  "SQL SELECT statement",     true  },
            { "params",        "array",   "Positional parameters",    false },
            { "limit",         "integer", "Max rows to return",       false }
        },
        "object"
    });

    // db_execute
    registerTool(MCPToolDefinition {
        "db_execute",
        "Execute INSERT/UPDATE/DELETE/DDL",
        {
            { "connection_id", "string", "Connection identifier", true  },
            { "sql",           "string", "SQL statement",         true  },
            { "params",        "array",  "Positional parameters", false }
        },
        "object"
    });

    // db_tables
    registerTool(MCPToolDefinition {
        "db_tables",
        "List tables and views in the database",
        {
            { "connection_id",  "string",  "Connection identifier",         true  },
            { "schema",         "string",  "Schema name (default public)",  false },
            { "include_views",  "boolean", "Include views",                 false }
        },
        "object"
    });

    // db_schema
    registerTool(MCPToolDefinition {
        "db_schema",
        "Get detailed schema of a table",
        {
            { "connection_id", "string", "Connection identifier", true  },
            { "table_name",    "string", "Table name",            true  },
            { "schema",        "string", "Schema name",           false }
        },
        "object"
    });

    // db_insert
    registerTool(MCPToolDefinition {
        "db_insert",
        "Insert one or multiple records into a table",
        {
            { "connection_id", "string", "Connection identifier",          true  },
            { "table",         "string", "Table name",                     true  },
            { "records",       "array",  "Array of objects to insert",     true  },
            { "on_conflict",   "string", "ON CONFLICT clause (optional)",  false }
        },
        "object"
    });

    // db_update
    registerTool(MCPToolDefinition {
        "db_update",
        "Update records matching a condition",
        {
            { "connection_id", "string", "Connection identifier",         true },
            { "table",         "string", "Table name",                    true },
            { "values",        "object", "Columns and new values",        true },
            { "where",         "object", "Condition columns and values",  true }
        },
        "object"
    });

    // db_delete
    registerTool(MCPToolDefinition {
        "db_delete",
        "Delete records matching a condition",
        {
            { "connection_id", "string", "Connection identifier",        true },
            { "table",         "string", "Table name",                   true },
            { "where",         "object", "Condition columns and values", true }
        },
        "object"
    });

    // db_export
    registerTool(MCPToolDefinition {
        "db_export",
        "Export table or query result to a file",
        {
            { "connection_id", "string", "Connection identifier",         true  },
            { "source",        "string", "Table name or SELECT query",    true  },
            { "format",        "string", "csv/json/sql",                  true  },
            { "output_path",   "string", "Destination file path",         true  },
            { "params",        "array",  "Query parameters",              false }
        },
        "object"
    });

    // db_import
    registerTool(MCPToolDefinition {
        "db_import",
        "Import data from a file into a table",
        {
            { "connection_id", "string",  "Connection identifier",                  true  },
            { "table",         "string",  "Target table name",                      true  },
            { "source_path",   "string",  "Source file path",                       true  },
            { "format",        "string",  "csv/json/sql (auto-detect by extension)", false },
            { "mode",          "string",  "insert/replace/append",                  false },
            { "header",        "boolean", "CSV: first row is header",               false }
        },
        "object"
    });

    // db_transactions
    registerTool(MCPToolDefinition {
        "db_transactions",
        "Manage database transactions",
        {
            { "connection_id",  "string", "Connection identifier",        true  },
            { "action",         "string", "begin/commit/rollback",        true  },
            { "transaction_id", "string", "Transaction ID for rollback",  false }
        },
        "object"
    });

    // db_optimize
    registerTool(MCPToolDefinition {
        "db_optimize",
        "Run database maintenance operations",
        {
            { "connection_id", "string", "Connection identifier",              true  },
            { "operation",     "string", "analyze/vacuum/reindex",             true  },
            { "table",         "string", "Specific table (optional)",          false }
        },
        "object"
    });

    // db_list_accounts
    registerTool(MCPToolDefinition {
        "db_list_accounts",
        "List pre-configured database accounts available for connection. "
        "Returns name, type, host, port, database for each account. "
        "Credentials (username/password) are not included.",
        {},
        "object"
    });

    // db_connect_account
    registerTool(MCPToolDefinition {
        "db_connect_account",
        "Open a connection using a pre-configured account by name",
        {
            { "account_name",    "string", "Account name from db_list_accounts", true  },
            { "connection_name", "string", "Unique identifier for this session connection (optional, defaults to account name)", false }
        },
        "object"
    });

    // db_account_create
    registerTool(MCPToolDefinition {
        "db_account_create",
        "Create a new database account and save it to the configuration file",
        {
            { "name",        "string",  "Unique account name",                        true  },
            { "description", "string",  "Human-readable description of this account", false },
            { "db_type",     "string",  "postgres / mysql / sqlite / mssql",          true  },
            { "database",    "string",  "Database name or file path (SQLite)",         true  },
            { "host",        "string",  "Host (default empty)",                        false },
            { "port",        "integer", "Port number",                                 false },
            { "username",    "string",  "Username",                                    false },
            { "password",    "string",  "Password",                                    false },
            { "ssl_mode",    "string",  "disable / request / require",                 false }
        },
        "object"
    });

    // db_account_update
    registerTool(MCPToolDefinition {
        "db_account_update",
        "Update an existing account in the configuration file. Only provided fields are changed.",
        {
            { "name",        "string",  "Name of the account to update",               true  },
            { "new_name",    "string",  "New account name (optional rename)",           false },
            { "description", "string",  "New description",                              false },
            { "db_type",     "string",  "New database type",                            false },
            { "database",    "string",  "New database name or file path",               false },
            { "host",        "string",  "New host",                                     false },
            { "port",        "integer", "New port",                                     false },
            { "username",    "string",  "New username",                                 false },
            { "password",    "string",  "New password",                                 false },
            { "ssl_mode",    "string",  "New SSL mode",                                 false }
        },
        "object"
    });

    // db_account_delete
    registerTool(MCPToolDefinition {
        "db_account_delete",
        "Delete an account from the configuration file",
        {
            { "name", "string", "Name of the account to delete", true }
        },
        "object"
    });

    // db_columns
    registerTool(MCPToolDefinition {
        "db_columns",
        "Get detailed column info: type, nullable, default, primary key, foreign keys, comments",
        {
            { "connection_id", "string", "Connection identifier", true  },
            { "table_name",    "string", "Table name",            true  },
            { "schema",        "string", "Schema name",           false }
        },
        "object"
    });

    // db_execute_batch
    registerTool(MCPToolDefinition {
        "db_execute_batch",
        "Execute multiple SQL statements separated by semicolons in a single call",
        {
            { "connection_id",    "string",  "Connection identifier",                  true  },
            { "sql",              "string",  "One or more SQL statements separated by semicolons", true },
            { "stop_on_error",    "boolean", "Stop execution on first error (default true)", false }
        },
        "object"
    });

    // db_explain
    registerTool(MCPToolDefinition {
        "db_explain",
        "Show query execution plan (EXPLAIN / EXPLAIN ANALYZE)",
        {
            { "connection_id", "string",  "Connection identifier",                  true  },
            { "sql",           "string",  "SELECT query to explain",                true  },
            { "analyze",       "boolean", "Run EXPLAIN ANALYZE (executes the query)", false }
        },
        "object"
    });

    // db_dump
    registerTool(MCPToolDefinition {
        "db_dump",
        "Dump database structure and data to a file (SQL or JSON)",
        {
            { "connection_id", "string", "Connection identifier",                          true  },
            { "output_path",   "string", "Destination file path",                          true  },
            { "format",        "string", "sql / json (default sql)",                       false },
            { "tables",        "array",  "List of table names to dump (default: all)",     false }
        },
        "object"
    });

    // db_restore
    registerTool(MCPToolDefinition {
        "db_restore",
        "Restore database from a dump file (SQL or JSON)",
        {
            { "connection_id", "string", "Connection identifier",                         true  },
            { "source_path",   "string", "Path to dump file",                             true  },
            { "mode",          "string", "append / replace (default append)",             false }
        },
        "object"
    });

    // db_indices
    registerTool(MCPToolDefinition {
        "db_indices",
        "List indexes of a table: name, type, columns, uniqueness",
        {
            { "connection_id", "string", "Connection identifier", true  },
            { "table_name",    "string", "Table name",            true  }
        },
        "object"
    });

    // db_triggers
    registerTool(MCPToolDefinition {
        "db_triggers",
        "List triggers of a table or create a new trigger",
        {
            { "connection_id", "string", "Connection identifier",                           true  },
            { "table_name",    "string", "Table name",                                      true  },
            { "create",        "string", "Full CREATE TRIGGER statement (omit to list only)", false }
        },
        "object"
    });

    // db_functions
    registerTool(MCPToolDefinition {
        "db_functions",
        "List stored functions/procedures or create a new one",
        {
            { "connection_id", "string", "Connection identifier",                              true  },
            { "create",        "string", "Full CREATE FUNCTION/PROCEDURE statement (omit to list)", false }
        },
        "object"
    });

    // db_size
    registerTool(MCPToolDefinition {
        "db_size",
        "Get database and per-table size statistics",
        {
            { "connection_id", "string", "Connection identifier", true }
        },
        "object"
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult DatabaseMCPServer::handleToolCall(const QString &name,
                                                const QJsonObject &args)
{
    if (name == "db_connect")      return toolDbConnect(args);
    if (name == "db_query")        return toolDbQuery(args);
    if (name == "db_execute")      return toolDbExecute(args);
    if (name == "db_tables")       return toolDbTables(args);
    if (name == "db_schema")       return toolDbSchema(args);
    if (name == "db_insert")       return toolDbInsert(args);
    if (name == "db_update")       return toolDbUpdate(args);
    if (name == "db_delete")       return toolDbDelete(args);
    if (name == "db_export")       return toolDbExport(args);
    if (name == "db_import")       return toolDbImport(args);
    if (name == "db_transactions") return toolDbTransactions(args);
    if (name == "db_optimize")         return toolDbOptimize(args);
    if (name == "db_list_accounts")    return toolDbListAccounts(args);
    if (name == "db_connect_account")  return toolDbConnectAccount(args);
    if (name == "db_account_create")   return toolDbAccountCreate(args);
    if (name == "db_account_update")   return toolDbAccountUpdate(args);
    if (name == "db_account_delete")   return toolDbAccountDelete(args);
    if (name == "db_columns")       return toolDbColumns(args);
    if (name == "db_execute_batch") return toolDbExecuteBatch(args);
    if (name == "db_explain")       return toolDbExplain(args);
    if (name == "db_dump")          return toolDbDump(args);
    if (name == "db_restore")       return toolDbRestore(args);
    if (name == "db_indices")       return toolDbIndices(args);
    if (name == "db_triggers")      return toolDbTriggers(args);
    if (name == "db_functions")     return toolDbFunctions(args);
    if (name == "db_size")          return toolDbSize(args);
    return MCPToolResult::error("Unknown tool: " + name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QVariantList DatabaseMCPServer::jsonArrayToVariantList(const QJsonArray &arr)
{
    QVariantList list;
    for (const QJsonValue &v : arr)
        list << v.toVariant();
    return list;
}

QVariantList DatabaseMCPServer::jsonObjectToVariantList(const QJsonObject &obj,
                                                        const QStringList &order)
{
    QVariantList list;
    for (const QString &k : order)
        list << obj.value(k).toVariant();
    return list;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool implementations
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult DatabaseMCPServer::toolDbConnect(const QJsonObject &args)
{
    ConnectionConfig cfg;
    cfg.name     = args.value("connection_name").toString();
    cfg.type     = args.value("db_type").toString();
    cfg.host     = args.value("host").toString("localhost");
    cfg.port     = args.value("port").toInt(0);
    cfg.database = args.value("database").toString();
    cfg.username = args.value("username").toString();
    cfg.password = args.value("password").toString();
    cfg.sslMode  = args.value("ssl_mode").toString();
    cfg.createdAt = QDateTime::currentDateTime();

    if (cfg.name.isEmpty())
        return MCPToolResult::error("connection_name is required");
    if (cfg.type.isEmpty())
        return MCPToolResult::error("db_type is required");
    if (cfg.database.isEmpty())
        return MCPToolResult::error("database is required");

    QString errMsg;
    if (!m_connMgr.addConnection(cfg, errMsg))
        return MCPToolResult::error(errMsg);

    QJsonObject result;
    result["success"]       = true;
    result["connection_id"] = cfg.name;
    result["message"]       = QString("Connected to %1://%2/%3")
                              .arg(cfg.type)
                              .arg(cfg.host.isEmpty() ? "localhost" : cfg.host)
                              .arg(cfg.database);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbQuery(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString sql    = args.value("sql").toString();
    const int     limit  = args.value("limit").toInt(0);

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (sql.isEmpty())
        return MCPToolResult::error("sql is required");

    QVariantList params;
    const QJsonValue paramsVal = args.value("params");
    if (paramsVal.isArray())
        params = jsonArrayToVariantList(paramsVal.toArray());

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    const QueryResult qr = m_executor.execSelect(db, sql, params, limit);

    if (!qr.success)
        return MCPToolResult::error(qr.errorMessage);

    QJsonObject result;
    result["success"]          = true;
    result["columns"]          = QJsonArray::fromStringList(qr.columns);
    result["rows"]             = qr.rows;
    result["row_count"]        = qr.rowCount;
    result["execution_time_ms"] = static_cast<int>(qr.executionTimeMs);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbExecute(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString sql    = args.value("sql").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (sql.isEmpty())
        return MCPToolResult::error("sql is required");

    QVariantList params;
    const QJsonValue paramsVal = args.value("params");
    if (paramsVal.isArray())
        params = jsonArrayToVariantList(paramsVal.toArray());

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    const QueryResult qr = m_executor.execNonSelect(db, sql, params);

    if (!qr.success)
        return MCPToolResult::error(qr.errorMessage);

    QJsonObject result;
    result["success"]          = true;
    result["affected_rows"]    = static_cast<int>(qr.affectedRows);
    result["execution_time_ms"] = static_cast<int>(qr.executionTimeMs);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbTables(const QJsonObject &args)
{
    const QString connId       = args.value("connection_id").toString();
    const QString schema       = args.value("schema").toString();
    const bool    includeViews = args.value("include_views").toBool(false);

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    const QJsonArray tables = m_inspector.listTables(db, schema, includeViews);

    QJsonObject result;
    result["tables"] = tables;
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbSchema(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString table  = args.value("table_name").toString();
    const QString schema = args.value("schema").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())
        return MCPToolResult::error("table_name is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_inspector.tableSchema(db, table, schema);

    if (result.contains("error"))
        return MCPToolResult::error(result.value("error").toString());

    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbInsert(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString table      = args.value("table").toString();
    const QJsonArray records = args.value("records").toArray();
    const QString onConflict = args.value("on_conflict").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())
        return MCPToolResult::error("table is required");
    if (records.isEmpty())
        return MCPToolResult::error("records array is required and must not be empty");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    const QString drv = db.driverName();

    int inserted = 0;
    QJsonArray errors;

    for (const QJsonValue &val : records) {
        if (!val.isObject()) { errors.append("Non-object record skipped"); continue; }
        const QJsonObject rec = val.toObject();
        const QStringList cols = rec.keys();

        QStringList placeholders;
        for (int i = 0; i < cols.size(); ++i) placeholders << "?";

        QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                      .arg(table)
                      .arg(cols.join(", "))
                      .arg(placeholders.join(", "));

        if (!onConflict.isEmpty()) {
            if (drv == "QPSQL" || drv == "QSQLITE") {
                sql += " ON CONFLICT " + onConflict;
            }
        }

        QSqlQuery q(db);
        q.prepare(sql);
        for (int i = 0; i < cols.size(); ++i)
            q.bindValue(i, rec.value(cols.at(i)).toVariant());

        if (q.exec()) ++inserted;
        else errors.append(q.lastError().text());
    }

    QJsonObject result;
    result["success"]       = (errors.isEmpty());
    result["inserted_rows"] = inserted;
    if (!errors.isEmpty()) result["errors"] = errors;
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbUpdate(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString table  = args.value("table").toString();
    const QJsonObject values = args.value("values").toObject();
    const QJsonObject where  = args.value("where").toObject();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())  return MCPToolResult::error("table is required");
    if (values.isEmpty()) return MCPToolResult::error("values is required");
    if (where.isEmpty())  return MCPToolResult::error("where is required");

    const QStringList setCols   = values.keys();
    const QStringList whereCols = where.keys();

    QStringList setParts;
    for (const QString &c : setCols) setParts << (c + " = ?");
    QStringList whereParts;
    for (const QString &c : whereCols) whereParts << (c + " = ?");

    const QString sql = QString("UPDATE %1 SET %2 WHERE %3")
                        .arg(table)
                        .arg(setParts.join(", "))
                        .arg(whereParts.join(" AND "));

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QSqlQuery q(db);
    q.prepare(sql);

    int idx = 0;
    for (const QString &c : setCols)   q.bindValue(idx++, values.value(c).toVariant());
    for (const QString &c : whereCols) q.bindValue(idx++, where.value(c).toVariant());

    if (!q.exec())
        return MCPToolResult::error(q.lastError().text());

    QJsonObject result;
    result["success"]      = true;
    result["updated_rows"] = q.numRowsAffected();
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbDelete(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString table      = args.value("table").toString();
    const QJsonObject where  = args.value("where").toObject();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty()) return MCPToolResult::error("table is required");
    if (where.isEmpty()) return MCPToolResult::error("where is required");

    const QStringList whereCols = where.keys();
    QStringList whereParts;
    for (const QString &c : whereCols) whereParts << (c + " = ?");

    const QString sql = QString("DELETE FROM %1 WHERE %2")
                        .arg(table)
                        .arg(whereParts.join(" AND "));

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QSqlQuery q(db);
    q.prepare(sql);
    for (int i = 0; i < whereCols.size(); ++i)
        q.bindValue(i, where.value(whereCols.at(i)).toVariant());

    if (!q.exec())
        return MCPToolResult::error(q.lastError().text());

    QJsonObject result;
    result["success"]      = true;
    result["deleted_rows"] = q.numRowsAffected();
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbExport(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString source     = args.value("source").toString();
    const QString format     = args.value("format").toString();
    const QString outputPath = args.value("output_path").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (source.isEmpty())     return MCPToolResult::error("source is required");
    if (format.isEmpty())     return MCPToolResult::error("format is required");
    if (outputPath.isEmpty()) return MCPToolResult::error("output_path is required");

    QVariantList params;
    if (args.value("params").isArray())
        params = jsonArrayToVariantList(args.value("params").toArray());

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_exporter.exportData(db, source, format, outputPath, params);

    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());

    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbImport(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString table      = args.value("table").toString();
    const QString sourcePath = args.value("source_path").toString();
    const QString format     = args.value("format").toString();
    const QString mode       = args.value("mode").toString("append");
    const bool    header     = args.value("header").toBool(true);

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())      return MCPToolResult::error("table is required");
    if (sourcePath.isEmpty()) return MCPToolResult::error("source_path is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_exporter.importData(db, table, sourcePath, format, mode, header);

    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());

    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbTransactions(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString action = args.value("action").toString();
    const QString txId   = args.value("transaction_id").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (action.isEmpty())
        return MCPToolResult::error("action is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_txCtrl.manage(db, connId, action, txId);

    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());

    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbOptimize(const QJsonObject &args)
{
    const QString connId    = args.value("connection_id").toString();
    const QString operation = args.value("operation").toString();
    const QString table     = args.value("table").toString();

    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (operation.isEmpty())
        return MCPToolResult::error("operation is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    const ConnectionConfig cfg = m_connMgr.config(connId);

    QJsonObject result = m_optimizer.optimize(db, cfg.type, operation, table);

    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());

    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbListAccounts(const QJsonObject &/*args*/)
{
    QString errMsg;
    m_accounts.load(m_configDir, errMsg);
    if (!errMsg.isEmpty())
        dbg(QString("[db_list_accounts] %1").arg(errMsg));

    QJsonObject result;
    result["success"]  = true;
    result["accounts"] = m_accounts.accountList();
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbConnectAccount(const QJsonObject &args)
{
    const QString accountName = args.value("account_name").toString();
    if (accountName.isEmpty())
        return MCPToolResult::error("account_name is required");

    if (!m_accounts.hasAccount(accountName))
        return MCPToolResult::error("Account not found: " + accountName);

    const ConnectionConfig cfg_src = m_accounts.account(accountName);

    // connection_name может быть переопределено, иначе = account_name
    ConnectionConfig cfg = cfg_src;
    const QString connName = args.value("connection_name").toString();
    if (!connName.isEmpty())
        cfg.name = connName;

    QString errMsg;
    if (!m_connMgr.addConnection(cfg, errMsg))
        return MCPToolResult::error(errMsg);

    QJsonObject result;
    result["success"]       = true;
    result["connection_id"] = cfg.name;
    result["message"]       = QString("Connected to %1://%2/%3")
                              .arg(cfg.type)
                              .arg(cfg.host.isEmpty() ? "localhost" : cfg.host)
                              .arg(cfg.database);
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Account CRUD helpers
// ─────────────────────────────────────────────────────────────────────────────

static ConnectionConfig accountFromArgs(const QJsonObject &args,
                                        const ConnectionConfig &base = ConnectionConfig{})
{
    ConnectionConfig cfg = base;
    if (args.contains("new_name"))    cfg.name        = args.value("new_name").toString();
    else if (args.contains("name"))   cfg.name        = args.value("name").toString();
    if (args.contains("description")) cfg.description = args.value("description").toString();
    if (args.contains("db_type"))     cfg.type        = args.value("db_type").toString();
    if (args.contains("host"))        cfg.host        = args.value("host").toString();
    if (args.contains("port"))        cfg.port        = args.value("port").toInt(base.port);
    if (args.contains("database"))    cfg.database    = args.value("database").toString();
    if (args.contains("username"))    cfg.username    = args.value("username").toString();
    if (args.contains("password"))    cfg.password    = args.value("password").toString();
    if (args.contains("ssl_mode"))    cfg.sslMode     = args.value("ssl_mode").toString();
    return cfg;
}

MCPToolResult DatabaseMCPServer::toolDbAccountCreate(const QJsonObject &args)
{
    const QString name = args.value("name").toString();
    if (name.isEmpty())
        return MCPToolResult::error("name is required");

    // Перечитываем актуальное состояние файла
    QString loadErr;
    m_accounts.load(m_configDir, loadErr);

    ConnectionConfig cfg = accountFromArgs(args);
    cfg.createdAt = QDateTime::currentDateTime();

    QString errMsg;
    if (!m_accounts.addAccount(cfg, errMsg))
        return MCPToolResult::error(errMsg);

    if (!m_accounts.save(m_configDir, errMsg))
        return MCPToolResult::error(errMsg);

    QJsonObject result;
    result["success"] = true;
    result["message"] = QString("Account '%1' created").arg(cfg.name);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbAccountUpdate(const QJsonObject &args)
{
    const QString name = args.value("name").toString();
    if (name.isEmpty())
        return MCPToolResult::error("name is required");

    QString loadErr;
    m_accounts.load(m_configDir, loadErr);

    if (!m_accounts.hasAccount(name))
        return MCPToolResult::error("Account not found: " + name);

    // Берём текущие значения и поверх накладываем только переданные поля
    ConnectionConfig updated = accountFromArgs(args, m_accounts.account(name));

    QString errMsg;
    if (!m_accounts.updateAccount(name, updated, errMsg))
        return MCPToolResult::error(errMsg);

    if (!m_accounts.save(m_configDir, errMsg))
        return MCPToolResult::error(errMsg);

    QJsonObject result;
    result["success"] = true;
    result["message"] = QString("Account '%1' updated").arg(name);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbAccountDelete(const QJsonObject &args)
{
    const QString name = args.value("name").toString();
    if (name.isEmpty())
        return MCPToolResult::error("name is required");

    QString loadErr;
    m_accounts.load(m_configDir, loadErr);

    QString errMsg;
    if (!m_accounts.removeAccount(name, errMsg))
        return MCPToolResult::error(errMsg);

    if (!m_accounts.save(m_configDir, errMsg))
        return MCPToolResult::error(errMsg);

    QJsonObject result;
    result["success"] = true;
    result["message"] = QString("Account '%1' deleted").arg(name);
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// New tools implementations
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult DatabaseMCPServer::toolDbColumns(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString table  = args.value("table_name").toString();
    const QString schema = args.value("schema").toString();
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())
        return MCPToolResult::error("table_name is required");
    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result;
    result["success"] = true;
    result["table"]   = table;
    result["columns"] = m_inspector.tableColumns(db, table, schema);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbExecuteBatch(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString sql        = args.value("sql").toString();
    const bool    stopOnErr  = args.value("stop_on_error").toBool(true);
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (sql.isEmpty())
        return MCPToolResult::error("sql is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);

    // Разбиваем на отдельные операторы по ';'
    QStringList stmts;
    QString cur;
    for (const QChar &ch : sql) {
        if (ch == ';') {
            cur = cur.trimmed();
            if (!cur.isEmpty()) { stmts << cur; cur.clear(); }
        } else {
            cur += ch;
        }
    }
    cur = cur.trimmed();
    if (!cur.isEmpty()) stmts << cur;

    int executed = 0;
    qint64 totalAffected = 0;
    QJsonArray errors;

    for (const QString &stmt : stmts) {
        QSqlQuery q(db);
        if (q.exec(stmt)) {
            ++executed;
            if (q.numRowsAffected() > 0) totalAffected += q.numRowsAffected();
        } else {
            errors.append(QJsonObject {
                { "statement", stmt },
                { "error",     q.lastError().text() }
            });
            if (stopOnErr) break;
        }
    }

    QJsonObject result;
    result["success"]          = errors.isEmpty();
    result["statements_total"] = (int)stmts.size();
    result["statements_ok"]    = executed;
    result["affected_rows"]    = (int)totalAffected;
    if (!errors.isEmpty()) result["errors"] = errors;
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbExplain(const QJsonObject &args)
{
    const QString connId  = args.value("connection_id").toString();
    const QString sql     = args.value("sql").toString();
    const bool    analyze = args.value("analyze").toBool(false);
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (sql.isEmpty())
        return MCPToolResult::error("sql is required");
    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_inspector.explainQuery(db, sql, analyze);
    if (result.contains("error"))
        return MCPToolResult::error(result.value("error").toString());
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbDump(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString outputPath = args.value("output_path").toString();
    const QString format     = args.value("format").toString("sql");
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (outputPath.isEmpty())
        return MCPToolResult::error("output_path is required");

    QStringList tables;
    const QJsonValue tablesVal = args.value("tables");
    if (tablesVal.isArray()) {
        for (const QJsonValue &v : tablesVal.toArray())
            tables << v.toString();
    }

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_exporter.dumpDatabase(db, outputPath, tables, format);
    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbRestore(const QJsonObject &args)
{
    const QString connId     = args.value("connection_id").toString();
    const QString sourcePath = args.value("source_path").toString();
    const QString mode       = args.value("mode").toString("append");
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (sourcePath.isEmpty())
        return MCPToolResult::error("source_path is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_exporter.restoreDatabase(db, sourcePath, mode);
    if (!result.value("success").toBool())
        return MCPToolResult::error(result.value("error").toString());
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbIndices(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString table  = args.value("table_name").toString();
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())
        return MCPToolResult::error("table_name is required");
    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result;
    result["success"] = true;
    result["table"]   = table;
    result["indices"] = m_inspector.tableIndices(db, table);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbTriggers(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString table  = args.value("table_name").toString();
    const QString create = args.value("create").toString();
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    if (table.isEmpty())
        return MCPToolResult::error("table_name is required");

    QSqlDatabase db = m_connMgr.getDatabase(connId);

    if (!create.isEmpty()) {
        const QueryResult qr = m_executor.execNonSelect(db, create);
        if (!qr.success)
            return MCPToolResult::error(qr.errorMessage);
        QJsonObject result;
        result["success"] = true;
        result["message"] = "Trigger created";
        return MCPToolResult::okJson(result);
    }

    QJsonObject result;
    result["success"]  = true;
    result["table"]    = table;
    result["triggers"] = m_inspector.tableTriggers(db, table);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbFunctions(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    const QString create = args.value("create").toString();
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);

    QSqlDatabase db = m_connMgr.getDatabase(connId);

    if (!create.isEmpty()) {
        const QueryResult qr = m_executor.execNonSelect(db, create);
        if (!qr.success)
            return MCPToolResult::error(qr.errorMessage);
        QJsonObject result;
        result["success"] = true;
        result["message"] = "Function/procedure created";
        return MCPToolResult::okJson(result);
    }

    QJsonObject result;
    result["success"]   = true;
    result["functions"] = m_inspector.listFunctions(db);
    return MCPToolResult::okJson(result);
}

MCPToolResult DatabaseMCPServer::toolDbSize(const QJsonObject &args)
{
    const QString connId = args.value("connection_id").toString();
    if (!m_connMgr.hasConnection(connId))
        return MCPToolResult::error("Connection not found: " + connId);
    QSqlDatabase db = m_connMgr.getDatabase(connId);
    QJsonObject result = m_inspector.dbSize(db);
    if (result.contains("error"))
        return MCPToolResult::error(result.value("error").toString());
    result["success"] = true;
    return MCPToolResult::okJson(result);
}
