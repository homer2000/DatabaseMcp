# Architecture

## Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    QMCPServer (Base)                        │
│  - JSON-RPC 2.0                                           │
│  - stdin/stdout communication                             │
│  - Tool registration                                      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  DatabaseMCPServer                          │
│  - Tool implementations                                     │
│  - Business logic                                         │
└─────────────────────────────────────────────────────────────┘
         │         │         │         │         │
         ▼         ▼         ▼         ▼         ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ Connection  │ │ Query       │ │ Schema      │ │ Data        │ │ Optimize    │
│ Manager     │ │ Executor    │ │ Inspector   │ │ Exporter    │ │ Engine      │
└─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘
```

## Components

### Core
- **QMCPServer**: Базовый MCP фреймворк
- **DatabaseMCPServer**: Главный сервер с бизнес-логикой

### Services
- **ConnectionManager**: Управление соединениями с балансировкой
- **QueryExecutor**: Выполнение SQL с таймингом
- **SchemaInspector**: Интроспекция БД
- **DataExporter**: Экспорт/импорт данных
- **TransactionController**: Управление транзакциями
- **OptimizingEngine**: Оптимизация запросов
- **AccountsConfig**: Конфигурация аккаунтов

## Data Flow

1. Client → JSON-RPC request
2. QMCPServer → Parse message
3. DatabaseMCPServer → Route to tool
4. Service → Execute operation
5. Response → JSON-RPC response