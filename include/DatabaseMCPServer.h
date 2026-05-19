#pragma once

#include "QMCPServer.h"
#include "ConnectionManager.h"
#include "QueryExecutor.h"
#include "SchemaInspector.h"
#include "DataExporter.h"
#include "TransactionController.h"
#include "OptimizingEngine.h"
#include "AccountsConfig.h"

#include <QJsonObject>

class DatabaseMCPServer : public QMCPServer
{
    Q_OBJECT
public:
    explicit DatabaseMCPServer(const QString &configDir, QObject *parent = nullptr);

protected:
    MCPToolResult handleToolCall(const QString &name,
                                 const QJsonObject &args) override;
    void onInitialized() override;

private:
    MCPToolResult toolDbConnect(const QJsonObject &args);
    MCPToolResult toolDbConnectAccount(const QJsonObject &args);
    MCPToolResult toolDbAccountCreate(const QJsonObject &args);
    MCPToolResult toolDbAccountUpdate(const QJsonObject &args);
    MCPToolResult toolDbAccountDelete(const QJsonObject &args);
    MCPToolResult toolDbListAccounts(const QJsonObject &args);
    MCPToolResult toolDbQuery(const QJsonObject &args);
    MCPToolResult toolDbExecute(const QJsonObject &args);
    MCPToolResult toolDbTables(const QJsonObject &args);
    MCPToolResult toolDbSchema(const QJsonObject &args);
    MCPToolResult toolDbInsert(const QJsonObject &args);
    MCPToolResult toolDbUpdate(const QJsonObject &args);
    MCPToolResult toolDbDelete(const QJsonObject &args);
    MCPToolResult toolDbExport(const QJsonObject &args);
    MCPToolResult toolDbImport(const QJsonObject &args);
    MCPToolResult toolDbTransactions(const QJsonObject &args);
    MCPToolResult toolDbOptimize(const QJsonObject &args);
    MCPToolResult toolDbColumns(const QJsonObject &args);
    MCPToolResult toolDbExecuteBatch(const QJsonObject &args);
    MCPToolResult toolDbExplain(const QJsonObject &args);
    MCPToolResult toolDbDump(const QJsonObject &args);
    MCPToolResult toolDbRestore(const QJsonObject &args);
    MCPToolResult toolDbIndices(const QJsonObject &args);
    MCPToolResult toolDbTriggers(const QJsonObject &args);
    MCPToolResult toolDbFunctions(const QJsonObject &args);
    MCPToolResult toolDbSize(const QJsonObject &args);

    static QVariantList jsonArrayToVariantList(const QJsonArray &arr);
    static QVariantList jsonObjectToVariantList(const QJsonObject &obj,
                                                const QStringList &order);

    QString               m_configDir;
    AccountsConfig        m_accounts;
    ConnectionManager     m_connMgr;
    QueryExecutor         m_executor;
    SchemaInspector       m_inspector;
    DataExporter          m_exporter;
    TransactionController m_txCtrl;
    OptimizingEngine      m_optimizer;
};
