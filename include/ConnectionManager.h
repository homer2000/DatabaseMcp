#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QDateTime>
#include <QSqlDatabase>

struct ConnectionConfig {
    QString   name;
    QString   description;
    QString   type;
    QString   host;
    int       port      = 0;
    QString   database;
    QString   username;
    QString   password;
    QString   sslMode;
    QDateTime createdAt;
};

class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager() override;

    bool          addConnection(const ConnectionConfig &cfg, QString &errorMsg);
    bool          hasConnection(const QString &name) const;
    QSqlDatabase  getDatabase(const QString &name) const;
    bool          removeConnection(const QString &name);
    QStringList   connectionNames() const;
    ConnectionConfig config(const QString &name) const;

private:
    mutable QMutex              m_mutex;
    QMap<QString, ConnectionConfig> m_configs;

    static QString qtDriver(const QString &dbType);
    static int     defaultPort(const QString &dbType);
};
