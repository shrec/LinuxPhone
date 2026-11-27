#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "iconprovider.h"

int main(int argc, char *argv[])
{
    // Runs straight on KMS; REVIVE_SCALE comes from the device (/etc/revive/shell.env).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "eglfs");
    qputenv("QT_QPA_EGLFS_HIDECURSOR", "1");
    const int scale = qMax(1, qEnvironmentVariableIntValue("REVIVE_SCALE"));
    qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));

    QGuiApplication app(argc, argv);
    QIcon::setThemeName("hicolor");
    QQmlApplicationEngine engine;
    engine.addImageProvider("icon", new IconProvider);
    engine.rootContext()->setContextProperty("outputScale", scale);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Revive.Shell", "Main");
    return app.exec();
}
