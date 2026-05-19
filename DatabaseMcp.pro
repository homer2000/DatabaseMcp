QT       += core sql
QT       -= gui

TARGET   = DatabaseMCPServer
TEMPLATE = app
CONFIG   += console
CONFIG   -= app_bundle
CONFIG   += c++17

greaterThan(QT_MAJOR_VERSION, 5) {
} else {
    DEFINES += QT_DEPRECATED_WARNINGS
}

INCLUDEPATH += $$PWD/include

SOURCES += \
    src/main.cpp \
    src/QMCPServer.cpp \
    src/DatabaseMCPServer.cpp \
    src/ConnectionManager.cpp \
    src/QueryExecutor.cpp \
    src/SchemaInspector.cpp \
    src/DataExporter.cpp \
    src/TransactionController.cpp \
    src/OptimizingEngine.cpp \
    src/AccountsConfig.cpp

HEADERS += \
    include/QMCPServer.h \
    include/DatabaseMCPServer.h \
    include/ConnectionManager.h \
    include/QueryExecutor.h \
    include/SchemaInspector.h \
    include/DataExporter.h \
    include/TransactionController.h \
    include/OptimizingEngine.h \
    include/AccountsConfig.h
