#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// One launchable app, from a freedesktop .desktop file.
struct AppEntry {
    QString id;       // file name without ".desktop", e.g. "foot" or "org.gnome.Calculator"
    QString name;     // localized Name=
    QString icon;     // Icon=: theme name or absolute path
    QString exec;     // raw Exec=
    QString wmClass;  // StartupWMClass=
    bool terminal = false;
};

namespace DesktopEntry {
// The [Desktop Entry] group, or nullopt for entries a launcher must not show:
// Type != Application, NoDisplay=true, Hidden=true, or no Name/Exec.
std::optional<AppEntry> parse(const QString &text, const QString &id, const QString &locale);
// Showable apps in dirs, sorted by name. An id in an earlier dir shadows later dirs (XDG order),
// including when the earlier one is hidden.
QList<AppEntry> scan(const QStringList &dirs, const QString &locale);
// Exec= as argv: quoting resolved, field codes (%f %U ...) removed, %% -> %.
QStringList commandLine(const QString &exec);
// Arguments for `systemd-run`: the app runs in its own user scope unit; Terminal=true apps run in foot.
QStringList systemdRunArgs(const AppEntry &e, const QString &unitSuffix);
// Whether a Wayland toplevel's app_id belongs to this entry (id or StartupWMClass, case-insensitive).
bool matchesAppId(const AppEntry &e, const QString &appId);
}
