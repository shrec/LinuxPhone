#include "appmodel.h"
#include <QLocale>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>

AppModel::AppModel(QObject *parent) : QAbstractListModel(parent) { reload(); }

int AppModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_apps.size()); }

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size()) return {};
    const AppEntry &e = m_apps.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case IconRole: return e.icon;
    case AppIdRole: return e.id;
    }
    return {};
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "icon"}, {AppIdRole, "appId"}};
}

void AppModel::reload()
{
    // REVIVE_APP_DIRS (colon-separated) overrides the XDG locations, for testing.
    const QString env = qEnvironmentVariable("REVIVE_APP_DIRS");
    const QStringList dirs = env.isEmpty()
        ? QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)
        : env.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    beginResetModel();
    m_apps = DesktopEntry::scan(dirs, QLocale::system().name());
    endResetModel();
}

const AppEntry *AppModel::find(const QString &appId) const
{
    for (const AppEntry &e : m_apps)
        if (e.id == appId) return &e;
    return nullptr;
}

bool AppModel::launch(const QString &appId)
{
    const AppEntry *e = find(appId);
    if (!e) return false;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", "wayland-0");
    env.insert("QT_QPA_PLATFORM", "wayland");
    env.insert("GDK_BACKEND", "wayland");
    env.insert("SDL_VIDEODRIVER", "wayland");
    env.insert("MOZ_ENABLE_WAYLAND", "1");
    env.insert("XDG_SESSION_TYPE", "wayland");
    env.remove("QT_SCALE_FACTOR");  // apps get their scale from wl_output, not from the shell's setting
    QProcess p;
    p.setProgram("systemd-run");
    p.setArguments(DesktopEntry::systemdRunArgs(*e, QString::number(QRandomGenerator::global()->generate(), 16)));
    p.setProcessEnvironment(env);
    return p.startDetached();
}

bool AppModel::matches(const QString &appId, const QString &waylandAppId) const
{
    const AppEntry *e = find(appId);
    return e && DesktopEntry::matchesAppId(*e, waylandAppId);
}
