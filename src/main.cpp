#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include "DatabaseMCPServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("DatabaseMCPServer");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configDirOption(
        QStringList() << "c" << "config-dir",
        "Directory with configuration files (accounts.json). "
        "Defaults to the directory of the executable.",
        "path");
    parser.addOption(configDirOption);
    parser.process(app);

    QString configDir;
    if (parser.isSet(configDirOption)) {
        configDir = parser.value(configDirOption);
    } else {
        configDir = QFileInfo(app.applicationFilePath()).absolutePath();
    }

    DatabaseMCPServer server(configDir);
    server.start();

    return app.exec();
}
