#include "AccountsConfig.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>

AccountsConfig::AccountsConfig(QObject *parent)
    : QObject(parent)
{}

bool AccountsConfig::load(const QString &configDir, QString &errorMsg)
{
    const QString path = configDir + "/accounts.json";
    QFile file(path);
    m_accounts.clear();

    if (!file.exists()) {
        QFile newFile(path);
        if (newFile.open(QIODevice::WriteOnly | QIODevice::Text))
            newFile.write("[\n]\n");
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QString("Cannot open accounts file: %1").arg(file.errorString());
        return false;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &pe);
    file.close();

    if (pe.error != QJsonParseError::NoError) {
        errorMsg = QString("Parse error in accounts file: %1").arg(pe.errorString());
        return false;
    }
    if (!doc.isArray()) {
        errorMsg = "accounts file: root element must be a JSON array";
        return false;
    }

    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject()) continue;
        const ConnectionConfig cfg = parseEntry(val.toObject());
        if (cfg.name.isEmpty()) continue;
        m_accounts.append(cfg);
    }
    return true;
}

bool AccountsConfig::save(const QString &configDir, QString &errorMsg) const
{
    const QString path = configDir + "/accounts.json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QString("Cannot write accounts file: %1").arg(file.errorString());
        return false;
    }
    QJsonArray arr;
    for (const ConnectionConfig &cfg : m_accounts)
        arr.append(entryToJson(cfg));
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

ConnectionConfig AccountsConfig::parseEntry(const QJsonObject &obj)
{
    ConnectionConfig cfg;
    cfg.name        = obj.value("name").toString();
    cfg.description = obj.value("description").toString();
    cfg.type        = obj.value("db_type").toString();
    cfg.host        = obj.value("host").toString();
    cfg.port        = obj.value("port").toInt(0);
    cfg.database    = obj.value("database").toString();
    cfg.username    = obj.value("username").toString();
    cfg.password    = obj.value("password").toString();
    cfg.sslMode     = obj.value("ssl_mode").toString();
    cfg.createdAt   = QDateTime::currentDateTime();
    return cfg;
}

QJsonObject AccountsConfig::entryToJson(const ConnectionConfig &cfg)
{
    QJsonObject obj;
    obj["name"]        = cfg.name;
    obj["description"] = cfg.description;
    obj["db_type"]     = cfg.type;
    obj["host"]        = cfg.host;
    obj["port"]        = cfg.port;
    obj["database"]    = cfg.database;
    obj["username"]    = cfg.username;
    obj["password"]    = cfg.password;
    obj["ssl_mode"]    = cfg.sslMode;
    return obj;
}

QJsonArray AccountsConfig::accountList() const
{
    QJsonArray arr;
    for (const ConnectionConfig &cfg : m_accounts) {
        QJsonObject obj;
        obj["name"]        = cfg.name;
        obj["description"] = cfg.description;
        obj["db_type"]     = cfg.type;
        obj["host"]        = cfg.host;
        obj["port"]        = cfg.port;
        obj["database"]    = cfg.database;
        obj["ssl_mode"]    = cfg.sslMode;
        // username и password намеренно не включаются
        arr.append(obj);
    }
    return arr;
}

bool AccountsConfig::hasAccount(const QString &name) const
{
    for (const ConnectionConfig &cfg : m_accounts)
        if (cfg.name == name) return true;
    return false;
}

ConnectionConfig AccountsConfig::account(const QString &name) const
{
    for (const ConnectionConfig &cfg : m_accounts)
        if (cfg.name == name) return cfg;
    return ConnectionConfig{};
}

QList<ConnectionConfig> AccountsConfig::allAccounts() const
{
    return m_accounts;
}

bool AccountsConfig::addAccount(const ConnectionConfig &cfg, QString &errorMsg)
{
    if (cfg.name.isEmpty()) {
        errorMsg = "Account name must not be empty";
        return false;
    }
    if (hasAccount(cfg.name)) {
        errorMsg = QString("Account already exists: %1").arg(cfg.name);
        return false;
    }
    m_accounts.append(cfg);
    return true;
}

bool AccountsConfig::updateAccount(const QString &name,
                                   const ConnectionConfig &cfg,
                                   QString &errorMsg)
{
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts[i].name == name) {
            // Если имя меняется — проверяем что новое не занято
            if (cfg.name != name && hasAccount(cfg.name)) {
                errorMsg = QString("Account name already in use: %1").arg(cfg.name);
                return false;
            }
            m_accounts[i] = cfg;
            return true;
        }
    }
    errorMsg = QString("Account not found: %1").arg(name);
    return false;
}

bool AccountsConfig::removeAccount(const QString &name, QString &errorMsg)
{
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts[i].name == name) {
            m_accounts.removeAt(i);
            return true;
        }
    }
    errorMsg = QString("Account not found: %1").arg(name);
    return false;
}
