#pragma once

#include <QObject>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "ConnectionManager.h"

class AccountsConfig : public QObject
{
    Q_OBJECT
public:
    explicit AccountsConfig(QObject *parent = nullptr);

    bool load(const QString &configDir, QString &errorMsg);
    bool save(const QString &configDir, QString &errorMsg) const;

    QJsonArray accountList() const;

    bool hasAccount(const QString &name) const;
    ConnectionConfig account(const QString &name) const;
    QList<ConnectionConfig> allAccounts() const;

    bool addAccount(const ConnectionConfig &cfg, QString &errorMsg);
    bool updateAccount(const QString &name, const ConnectionConfig &cfg, QString &errorMsg);
    bool removeAccount(const QString &name, QString &errorMsg);

private:
    QList<ConnectionConfig> m_accounts;

    static ConnectionConfig parseEntry(const QJsonObject &obj);
    static QJsonObject      entryToJson(const ConnectionConfig &cfg);
};
