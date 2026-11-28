#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include "iconprovider.h"

int main(int argc, char *argv[])
{
    // Runs as a layer-shell client on phoc: wayland platform, scale comes from wl_output.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "wayland");
    if (qEnvironmentVariableIsEmpty("QT_WAYLAND_SHELL_INTEGRATION"))
        qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");

    QGuiApplication app(argc, argv);

    // On-screen keyboard: phosh-osk-stevia (squeekboard's successor; squeekboard left Debian testing
    // with libgnome-desktop-3) shows itself on text-input focus (screen-keyboard-enabled=true).
    // Started as a transient user service, not a scope: forked from the shell (phoc's child) the OSK
    // aborted at startup, its glycin icon loader exiting early; spawned by the user manager it runs.
    // Skipped when it isn't installed.
    const QString osk = QStringLiteral("phosh-osk-stevia");
    if (!QStandardPaths::findExecutable(osk).isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("QT_WAYLAND_SHELL_INTEGRATION");  // set above for the shell itself; the OSK must not become one of ours
        QProcess p;
        p.setProgram("systemd-run");
        p.setArguments({"--user", "--quiet", "--collect", "--unit=revive-osk", "-E", "WAYLAND_DISPLAY",
                        "-E", "XDG_SESSION_TYPE", "-E", "FD_MESA_DEBUG", "-E", "GSK_RENDERER", osk});
        p.setProcessEnvironment(env);
        p.startDetached();
    }
    QIcon::setThemeName("hicolor");
    QQmlApplicationEngine engine;
    engine.addImageProvider("icon", new IconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Revive.Shell", "Main");
    return app.exec();
}
