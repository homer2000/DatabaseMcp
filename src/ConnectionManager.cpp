#include "ConnectionManager.h"

#include <QMutexLocker>
#include <QSqlError>

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
{}

ConnectionManager::~ConnectionManager()
{
    QMutexLocker lock(&m_mutex);
    for (const QString &name : m_configs.keys()) {
        if (QSqlDatabase::contains(name))
            QSqlDatabase::removeDatabase(name);
    }
}

QString ConnectionManager::qtDriver(const QString &dbType)
{
    const QString t = dbType.toLower();
    if (t == "postgres" || t == "postgresql") return QStringLiteral("QPSQL");
    if (t == "mysql" || t == "mariadb")       return QStringLiteral("QMYSQL");
    if (t == "sqlite")                         return QStringLiteral("QSQLITE");
    if (t == "mssql" || t == "sqlserver")      return QStringLiteral("QODBC");
    return QString();
}

int ConnectionManager::defaultPort(const QString &dbType)
{
    const QString t = dbType.toLower();
    if (t == "postgres" || t == "postgresql") return 5432;
    if (t == "mysql" || t == "mariadb")       return 3306;
    if (t == "mssql" || t == "sqlserver")     return 1433;
    return 0;
}

bool ConnectionManager::addConnection(const ConnectionConfig &cfg, QString &errorMsg)
{
    QMutexLocker lock(&m_mutex);

    if (m_configs.contains(cfg.name)) {
        // Close & remove existing
        if (QSqlDatabase::contains(cfg.name))
            QSqlDatabase::removeDatabase(cfg.name);
        m_configs.remove(cfg.name);
    }

    const QString driver = qtDriver(cfg.type);
    if (driver.isEmpty()) {
        errorMsg = QString("Unsupported database type: %1").arg(cfg.type);
        return false;
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(driver, cfg.name);

        if (cfg.type.toLower() == "sqlite") {
            db.setDatabaseName(cfg.database);
        } else {
            db.setHostName(cfg.host.isEmpty() ? QStringLiteral("localhost") : cfg.host);
            int port = cfg.port > 0 ? cfg.port : defaultPort(cfg.type);
            if (port > 0) db.setPort(port);
            db.setDatabaseName(cfg.database);
            if (!cfg.username.isEmpty()) db.setUserName(cfg.username);
            if (!cfg.password.isEmpty()) db.setPassword(cfg.password);

            if (cfg.sslMode == "require" || cfg.sslMode == "request") {
                db.setConnectOptions(QStringLiteral("requiressl=1"));
            }
        }

        if (!db.open()) {
            errorMsg = db.lastError().text();
            QSqlDatabase::removeDatabase(cfg.name);
            return false;
        }
    }

    m_configs.insert(cfg.name, cfg);
    return true;
}

bool ConnectionManager::hasConnection(const QString &name) const
{
    QMutexLocker lock(&m_mutex);
    return m_configs.contains(name) && QSqlDatabase::contains(name);
}

QSqlDatabase ConnectionManager::getDatabase(const QString &name) const
{
    return QSqlDatabase::database(name, false);
}

bool ConnectionManager::removeConnection(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    if (!m_configs.contains(name)) return false;
    if (QSqlDatabase::contains(name))
        QSqlDatabase::removeDatabase(name);
    m_configs.remove(name);
    return true;
}

QStringList ConnectionManager::connectionNames() const
{
    QMutexLocker lock(&m_mutex);
    return m_configs.keys();
}

ConnectionConfig ConnectionManager::config(const QString &name) const
{
    QMutexLocker lock(&m_mutex);
    return m_configs.value(name);
}
