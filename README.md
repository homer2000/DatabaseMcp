# DatabaseMCPServer

💾 **DatabaseMCPServer** - Qt/C++ реализация MCP (Model Context Protocol) сервера для работы с базами данных.

## Возможности

- Поддержка множества СУБД: PostgreSQL, MySQL, SQLite, MSSQL
- JSON-RPC 2.0 протокол (MCP)
- Потокобезопасные соединения с базами данных
- Автоматическое определение схемы базы данных
- Экспорт данных в CSV/JSON/SQL форматы
- Управление транзакциями
- Оптимизация запросов (EXPLAIN, анализ)

## Установка

### Требования
- Qt 5.15.2+ или Qt 6.x
- QtSql модуль
- Драйверы БД (опционально, для нужной СУБД)

### Сборка

```bash
# qmake (рекомендуется для Qt проектов)
qmake DatabaseMcp.pro
make

# Или CMake
cmake -B build -S .
cmake --build build
```

## Настройка

1. Скопируйте пример конфигурации:
   ```bash
   cp accounts.json.example accounts.json
   ```

2. Отредактируйте `accounts.json` с учётными записями ваших баз данных:
   ```json
   {
     "accounts": [
       {
         "name": "my_postgres",
         "db_type": "postgres",
         "host": "localhost",
         "port": 5432,
         "database": "mydb",
         "username": "user",
         "password": "password",
         "ssl_mode": "require"
       }
     ]
   }
   ```

## Использование

```bash
./DatabaseMCPServer
```

Сервер будет слушать на порту 8080 и принимать MCP запросы.

## Поддерживаемые операции

| Категория | Операции |
|-----------|----------|
| Подключение | `db_connect`, `db_disconnect` |
| Запросы | `db_query`, `db_execute`, `db_explain` |
| Схема | `db_tables`, `db_columns`, `db_indices` |
| Данные | `db_insert`, `db_update`, `db_delete` |
| Экспорт | `db_export`, `db_dump` |

## Лицензия

MIT License - см. файл [LICENSE](LICENSE)

## Автор

Разработано для MCP экосистемы