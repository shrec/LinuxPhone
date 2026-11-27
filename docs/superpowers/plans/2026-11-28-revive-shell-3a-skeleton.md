# revive-shell 3a (skeleton) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The phone boots into revive's own graphical shell, a Qt Wayland compositor with a status bar and an app launcher, and runs Linux apps (first: the foot terminal) fullscreen inside it.

**Architecture:** One process, `revive-shell` (C++ core plus QML UI on Qt 6 Wayland Compositor), runs on KMS through eglfs as user `phone` in a real logind session. C++ provides the installed-app list (parsed from `.desktop` files) and launches apps in `systemd-run --user --scope` units. QML is the compositor plus status bar, launcher and fullscreen app view, with swipe-up to go home.

**Tech Stack:** Qt 6.11 (Debian forky): QtGui, QtQml, QtQuick, QtWaylandCompositor (QML), QtTest; CMake; systemd; Mesa freedreno.

**Spec:** `docs/superpowers/specs/2026-11-28-revive-shell-and-sdk-design.md` (phase 3a)

## Global Constraints
- Base system: Debian forky arm64. Qt comes from Debian; no other UI frameworks.
- revive's own code is MIT and contains no GPL code. Third-party apps ship as separate Debian packages.
- Look: background `#050806`, accent `#39ff88`, font JetBrains Mono, like the boot logo.
- Output scale: `SCALE=3` for the Mi A2 (`wl_output` scale is an integer, and 1080 px / 3 = 360 logical px).
  This supersedes the spec's 2.5.
- Dev target: the phone at `ssh phone@172.16.42.1` (sudo password `1234`). Develop by building on the
  phone over SSH, with no reflash. Only Task 6 rebuilds and flashes the image.
- Git: stage files by explicit path (never `git add -A`; AIWorkHub drops files in the repo) and push to `main`.
- When the shell fails repeatedly, the phone must fall back to the text console (getty and buffyboard), never a dead screen.

## Review Focus
1. `.desktop` files a launcher must not show (`NoDisplay=true`, `Hidden=true`, `Type=Link`, missing `Exec`) must not appear. Test in Task 1.
2. `Exec=` lines with field codes (`%U`, `%f`), quoted arguments and `%%` must become the right argv. Test in Task 1.
3. A `Terminal=true` app (btop, ranger) must open inside foot, not start invisibly. Test in Task 2.
4. An app that exits or crashes while shown must bring the user back to the launcher, not a black screen. Manual check in Task 4.
5. Tapping an app that is already running should bring its window back rather than start a second copy. Test in Task 2, manual check in Task 4.

---

## File Structure
```
shell/CMakeLists.txt            build: revive_core (pure logic, tested), revive-shell (exe + QML module), tests
shell/src/desktopentry.h/.cpp   AppEntry; parse/scan .desktop files; Exec -> argv; systemd-run args; app_id matching
shell/src/appmodel.h/.cpp       QML singleton AppModel: list model of apps + launch()
shell/src/iconprovider.h        image://icon/<name> from the icon theme
shell/src/main.cpp              platform/scale setup, engine, image provider
shell/qml/Theme.qml             singleton: colours, font
shell/qml/Main.qml              compositor, output window, app surfaces, swipe-up home
shell/qml/StatusBar.qml         top bar: "revive" + clock
shell/qml/Launcher.qml          app grid
shell/tests/tst_desktopentry.cpp
shell/dev-deploy.sh             build on the phone over SSH and restart the shell
rootfs/etc/systemd/system/revive-shell.service
build.sh, Dockerfile, devices/xiaomi-jasmine/device.conf   image integration (Task 6)
```

---

### Task 1: Desktop-entry parsing (`revive_core`) with tests, built on the phone

**Files:**
- Create: `shell/CMakeLists.txt`, `shell/src/desktopentry.h`, `shell/src/desktopentry.cpp`, `shell/tests/tst_desktopentry.cpp`, `shell/dev-deploy.sh`

**Interfaces:**
- Produces:
  - `struct AppEntry { QString id, name, icon, exec, wmClass; bool terminal = false; };`
  - `std::optional<AppEntry> DesktopEntry::parse(const QString &text, const QString &id, const QString &locale);`
  - `QList<AppEntry> DesktopEntry::scan(const QStringList &dirs, const QString &locale);`
  - `QStringList DesktopEntry::commandLine(const QString &exec);`

- [ ] **Step 1: Install build tools on the phone**

```bash
ssh phone@172.16.42.1 'echo 1234 | sudo -S apt-get install -y -q --no-install-recommends build-essential cmake qt6-base-dev qt6-declarative-dev'
```
Expected: ends with `Setting up qt6-declarative-dev ...`.

- [ ] **Step 2: Write `shell/CMakeLists.txt`** (core and tests only for now; Task 3 adds the executable)

```cmake
cmake_minimum_required(VERSION 3.22)
project(revive-shell LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Test)
qt_standard_project_setup(REQUIRES 6.5)

# Pure logic, no GUI: unit-tested on its own.
qt_add_library(revive_core STATIC src/desktopentry.cpp src/desktopentry.h)
target_include_directories(revive_core PUBLIC src)
target_link_libraries(revive_core PUBLIC Qt6::Core)

enable_testing()
qt_add_executable(tst_desktopentry tests/tst_desktopentry.cpp)
target_link_libraries(tst_desktopentry PRIVATE revive_core Qt6::Test)
add_test(NAME desktopentry COMMAND tst_desktopentry)
```

- [ ] **Step 3: Write the failing tests `shell/tests/tst_desktopentry.cpp`**

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "desktopentry.h"

static void write(const QString &path, const QByteArray &text) {
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(text);
}

class TstDesktopEntry : public QObject {
    Q_OBJECT
private slots:
    void parsesApplication() {
        auto e = DesktopEntry::parse("[Desktop Entry]\nType=Application\nName=Foot\nExec=foot\nIcon=foot\n"
                                     "StartupWMClass=FootTerm\nTerminal=false\n", "foot", "en_US");
        QVERIFY(e);
        QCOMPARE(e->id, QString("foot"));
        QCOMPARE(e->name, QString("Foot"));
        QCOMPARE(e->icon, QString("foot"));
        QCOMPARE(e->exec, QString("foot"));
        QCOMPARE(e->wmClass, QString("FootTerm"));
        QVERIFY(!e->terminal);
    }
    void skipsWhatALauncherMustNotShow_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("NoDisplay") << "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=true\n";
        QTest::newRow("Hidden") << "[Desktop Entry]\nType=Application\nName=A\nExec=a\nHidden=true\n";
        QTest::newRow("Link") << "[Desktop Entry]\nType=Link\nName=A\nURL=https://x\n";
        QTest::newRow("no Exec") << "[Desktop Entry]\nType=Application\nName=A\n";
        QTest::newRow("no Name") << "[Desktop Entry]\nType=Application\nExec=a\n";
    }
    void skipsWhatALauncherMustNotShow() {
        QFETCH(QString, text);
        QVERIFY(!DesktopEntry::parse(text, "a", "en_US"));
    }
    void ignoresOtherGroupsAndComments() {
        auto e = DesktopEntry::parse("# c\n[Desktop Entry]\nType=Application\nName=Real\nExec=real\n"
                                     "[Desktop Action new]\nName=Fake\nExec=fake\n", "r", "en_US");
        QVERIFY(e);
        QCOMPARE(e->name, QString("Real"));
        QCOMPARE(e->exec, QString("real"));
    }
    void prefersLocalizedName() {
        const QString t = "[Desktop Entry]\nType=Application\nName=Calculator\nName[ka]=კალკულატორი\nExec=c\n";
        QCOMPARE(DesktopEntry::parse(t, "c", "ka_GE.UTF-8")->name, QString("კალკულატორი"));
        QCOMPARE(DesktopEntry::parse(t, "c", "en_US")->name, QString("Calculator"));
    }
    void readsTerminalFlag() {
        QVERIFY(DesktopEntry::parse("[Desktop Entry]\nType=Application\nName=b\nExec=btop\nTerminal=true\n",
                                    "btop", "C")->terminal);
    }
    void commandLine_data() {
        QTest::addColumn<QString>("exec");
        QTest::addColumn<QStringList>("argv");
        QTest::newRow("plain") << "foot" << QStringList{"foot"};
        QTest::newRow("field codes dropped") << "firefox-esr %u" << QStringList{"firefox-esr"};
        QTest::newRow("several codes") << "app --new %F %i %c" << QStringList{"app", "--new"};
        QTest::newRow("quoted") << R"(sh -c "echo hi there")" << QStringList{"sh", "-c", "echo hi there"};
        QTest::newRow("escaped quote") << R"(a "say \"x\"")" << QStringList{"a", R"(say "x")"};
        QTest::newRow("percent") << "printf 100%%" << QStringList{"printf", "100%"};
        QTest::newRow("extra spaces") << "  a   b  " << QStringList{"a", "b"};
        QTest::newRow("empty quoted arg") << R"(a "")" << QStringList{"a", ""};
    }
    void commandLine() {
        QFETCH(QString, exec);
        QFETCH(QStringList, argv);
        QCOMPARE(DesktopEntry::commandLine(exec), argv);
    }
    void scanSortsAndEarlierDirShadowsLater() {
        QTemporaryDir user, sys;
        write(sys.filePath("b.desktop"), "[Desktop Entry]\nType=Application\nName=Bravo\nExec=b\n");
        write(sys.filePath("a.desktop"), "[Desktop Entry]\nType=Application\nName=alpha\nExec=a\n");
        write(sys.filePath("gone.desktop"), "[Desktop Entry]\nType=Application\nName=Gone\nExec=g\n");
        write(user.filePath("gone.desktop"), "[Desktop Entry]\nType=Application\nName=Gone\nExec=g\nHidden=true\n");
        write(sys.filePath("notes.txt"), "not a desktop file");
        const auto apps = DesktopEntry::scan({user.path(), sys.path()}, "C");
        QCOMPARE(apps.size(), 2);
        QCOMPARE(apps[0].name, QString("alpha"));
        QCOMPARE(apps[1].name, QString("Bravo"));
    }
};

QTEST_MAIN(TstDesktopEntry)
#include "tst_desktopentry.moc"
```

- [ ] **Step 4: Write a header-only stub so the test compiles, plus `shell/dev-deploy.sh`**

`shell/src/desktopentry.h`:
```cpp
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
}
```
`shell/src/desktopentry.cpp`:
```cpp
#include "desktopentry.h"
std::optional<AppEntry> DesktopEntry::parse(const QString &, const QString &, const QString &) { return std::nullopt; }
QList<AppEntry> DesktopEntry::scan(const QStringList &, const QString &) { return {}; }
QStringList DesktopEntry::commandLine(const QString &) { return {}; }
```
`shell/dev-deploy.sh`:
```sh
#!/bin/sh
# Build revive-shell on the phone, run its tests, install it and restart it.
#   ./shell/dev-deploy.sh [user@host]      (default phone@172.16.42.1; asks for the sudo password)
set -e
H=${1:-phone@172.16.42.1}
cd "$(dirname "$0")"
tar -cf - CMakeLists.txt src qml tests 2>/dev/null | ssh "$H" 'rm -rf ~/revive-shell-src && mkdir -p ~/revive-shell-src && tar -xf - -C ~/revive-shell-src'
ssh -t "$H" 'set -e; cd ~/revive-shell-src
  cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
  cmake --build build -j"$(nproc)"
  ctest --test-dir build --output-on-failure
  if [ -f build/revive-shell ]; then sudo cmake --install build --prefix /usr && sudo systemctl restart revive-shell 2>/dev/null || true; fi'
```
Run `chmod +x shell/dev-deploy.sh`.

- [ ] **Step 5: Build and run the tests on the phone to see them fail**

The agent can't type into `ssh -t`, so it runs the same thing non-interactively:
```bash
cd shell && tar -cf - CMakeLists.txt src tests | ssh phone@172.16.42.1 'rm -rf ~/revive-shell-src && mkdir -p ~/revive-shell-src && tar -xf - -C ~/revive-shell-src' && ssh phone@172.16.42.1 'cd ~/revive-shell-src && cmake -S . -B build >/dev/null && cmake --build build -j8 2>&1 | tail -2 && ctest --test-dir build --output-on-failure | tail -15'
```
Expected: builds; `parsesApplication`, `commandLine`, `scanSorts...` and others FAIL. (`skipsWhatALauncherMustNotShow` passes against the stub.)

- [ ] **Step 6: Implement `shell/src/desktopentry.cpp`**

```cpp
#include "desktopentry.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <algorithm>

std::optional<AppEntry> DesktopEntry::parse(const QString &text, const QString &id, const QString &locale)
{
    QHash<QString, QString> kv;
    bool inMain = false;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inMain = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (!inMain || eq <= 0)
            continue;
        kv.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    const auto isTrue = [&](const char *k) { return kv.value(QLatin1String(k)) == QLatin1String("true"); };
    if (kv.value("Type") != QLatin1String("Application") || isTrue("NoDisplay") || isTrue("Hidden"))
        return std::nullopt;

    // "ka_GE.UTF-8@x" -> try Name[ka_GE], then Name[ka], then Name.
    const QString loc = locale.section(QLatin1Char('.'), 0, 0).section(QLatin1Char('@'), 0, 0);
    const QString lang = loc.section(QLatin1Char('_'), 0, 0);
    QString name = kv.value("Name[" + loc + "]");
    if (name.isEmpty()) name = kv.value("Name[" + lang + "]");
    if (name.isEmpty()) name = kv.value("Name");
    const QString exec = kv.value("Exec");
    if (name.isEmpty() || exec.isEmpty())
        return std::nullopt;
    return AppEntry{id, name, kv.value("Icon"), exec, kv.value("StartupWMClass"), isTrue("Terminal")};
}

QList<AppEntry> DesktopEntry::scan(const QStringList &dirs, const QString &locale)
{
    QSet<QString> seen;
    QList<AppEntry> apps;
    for (const QString &dir : dirs) {
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString id = QFileInfo(path).completeBaseName();
            if (seen.contains(id))
                continue;
            seen.insert(id);  // before parsing: a hidden entry still shadows later dirs
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            if (auto e = parse(QString::fromUtf8(f.readAll()), id, locale))
                apps << *e;
        }
    }
    std::sort(apps.begin(), apps.end(), [](const AppEntry &a, const AppEntry &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return apps;
}

QStringList DesktopEntry::commandLine(const QString &exec)
{
    QStringList args;
    QString cur;
    bool quoted = false, started = false;  // started: a (possibly empty) quoted arg was opened
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec[i];
        if (quoted) {
            if (c == QLatin1Char('\\') && i + 1 < exec.size()) { cur += exec[++i]; continue; }
            if (c == QLatin1Char('"')) { quoted = false; continue; }
            cur += c;
            continue;
        }
        if (c == QLatin1Char('"')) { quoted = started = true; continue; }
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
            if (started || !cur.isEmpty()) args << cur;
            cur.clear();
            started = false;
            continue;
        }
        if (c == QLatin1Char('%') && i + 1 < exec.size()) {
            if (exec[++i] == QLatin1Char('%')) cur += QLatin1Char('%');  // other field codes are dropped
            continue;
        }
        cur += c;
    }
    if (started || !cur.isEmpty()) args << cur;
    return args;
}
```

- [ ] **Step 7: Run the tests on the phone**

Same command as Step 5. Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 8: Commit**

```bash
git add shell/CMakeLists.txt shell/src/desktopentry.h shell/src/desktopentry.cpp shell/tests/tst_desktopentry.cpp shell/dev-deploy.sh
git commit -m "shell: parse .desktop files into launchable apps, with tests"
```

---

### Task 2: Launch arguments and app_id matching

**Files:**
- Modify: `shell/src/desktopentry.h`, `shell/src/desktopentry.cpp`, `shell/tests/tst_desktopentry.cpp`

**Interfaces:**
- Consumes: `AppEntry`, `DesktopEntry::commandLine` (Task 1)
- Produces:
  - `QStringList DesktopEntry::systemdRunArgs(const AppEntry &e, const QString &unitSuffix);`
    Arguments for `systemd-run`: `--user --scope --collect --unit=app-revive-<safe id>-<suffix> -- <argv>`.
    `Terminal=true` apps get `foot --` in front of their argv.
  - `bool DesktopEntry::matchesAppId(const AppEntry &e, const QString &appId);`
    Case-insensitive match of a Wayland toplevel `app_id` against the entry id or StartupWMClass. Empty never matches.

- [ ] **Step 1: Add the failing tests** (inside `TstDesktopEntry`, before `};`)

```cpp
    void systemdRunArgs() {
        AppEntry foot{"foot", "Foot", "foot", "foot", "", false};
        QCOMPARE(DesktopEntry::systemdRunArgs(foot, "1a2b"),
                 (QStringList{"--user", "--scope", "--collect", "--unit=app-revive-foot-1a2b", "--", "foot"}));
    }
    void terminalAppsRunInFoot() {
        AppEntry btop{"btop", "btop++", "btop", "btop --utf-force", "", true};
        QCOMPARE(DesktopEntry::systemdRunArgs(btop, "9"),
                 (QStringList{"--user", "--scope", "--collect", "--unit=app-revive-btop-9", "--",
                              "foot", "--", "btop", "--utf-force"}));
    }
    void unitNameIsSanitized() {
        AppEntry odd{"my app+x@1", "Odd", "", "odd", "", false};
        QCOMPARE(DesktopEntry::systemdRunArgs(odd, "7").at(3), QString("--unit=app-revive-my_app_x_1-7"));
    }
    void matchesAppId() {
        AppEntry calc{"org.gnome.Calculator", "Calculator", "", "gnome-calculator", "", false};
        AppEntry ff{"firefox-esr", "Firefox", "", "firefox-esr", "firefox-esr", false};
        AppEntry klass{"telegram", "Telegram", "", "telegram-desktop", "TelegramDesktop", false};
        QVERIFY(DesktopEntry::matchesAppId(calc, "org.gnome.Calculator"));
        QVERIFY(DesktopEntry::matchesAppId(calc, "ORG.GNOME.CALCULATOR"));
        QVERIFY(DesktopEntry::matchesAppId(ff, "firefox-esr"));
        QVERIFY(DesktopEntry::matchesAppId(klass, "telegramdesktop"));
        QVERIFY(!DesktopEntry::matchesAppId(calc, ""));
        QVERIFY(!DesktopEntry::matchesAppId(calc, "foot"));
    }
```
Declare in `desktopentry.h` (inside `namespace DesktopEntry`):
```cpp
// Arguments for `systemd-run`: the app runs in its own user scope unit; Terminal=true apps run in foot.
QStringList systemdRunArgs(const AppEntry &e, const QString &unitSuffix);
// Whether a Wayland toplevel's app_id belongs to this entry (id or StartupWMClass, case-insensitive).
bool matchesAppId(const AppEntry &e, const QString &appId);
```
And add stubs to `desktopentry.cpp`:
```cpp
QStringList DesktopEntry::systemdRunArgs(const AppEntry &, const QString &) { return {}; }
bool DesktopEntry::matchesAppId(const AppEntry &, const QString &) { return false; }
```

- [ ] **Step 2: Run the tests to see them fail** (Task 1 Step 5 command). Expected: the four new tests FAIL, the Task 1 tests PASS.

- [ ] **Step 3: Implement** (replace the stubs)

```cpp
QStringList DesktopEntry::systemdRunArgs(const AppEntry &e, const QString &unitSuffix)
{
    QString safe = e.id;
    for (QChar &c : safe)  // systemd unit names allow [A-Za-z0-9:_.-]
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != QLatin1Char(':') && c != QLatin1Char('_')
            && c != QLatin1Char('.') && c != QLatin1Char('-'))
            c = QLatin1Char('_');
    QStringList argv = commandLine(e.exec);
    if (e.terminal)
        argv = QStringList{QStringLiteral("foot"), QStringLiteral("--")} + argv;
    return QStringList{QStringLiteral("--user"), QStringLiteral("--scope"), QStringLiteral("--collect"),
                       QStringLiteral("--unit=app-revive-%1-%2").arg(safe, unitSuffix), QStringLiteral("--")}
           + argv;
}

bool DesktopEntry::matchesAppId(const AppEntry &e, const QString &appId)
{
    if (appId.isEmpty())
        return false;
    return appId.compare(e.id, Qt::CaseInsensitive) == 0
           || (!e.wmClass.isEmpty() && appId.compare(e.wmClass, Qt::CaseInsensitive) == 0);
}
```

- [ ] **Step 4: Run the tests.** Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add shell/src/desktopentry.h shell/src/desktopentry.cpp shell/tests/tst_desktopentry.cpp
git commit -m "shell: systemd-run launch args (terminal apps in foot) and app_id matching"
```

---

### Task 3: AppModel, icon provider and the shell executable

**Files:**
- Create: `shell/src/appmodel.h`, `shell/src/appmodel.cpp`, `shell/src/iconprovider.h`, `shell/src/main.cpp`, `shell/qml/Theme.qml`, `shell/qml/Main.qml` (temporary: launcher list only)
- Modify: `shell/CMakeLists.txt`

**Interfaces:**
- Consumes: `DesktopEntry::scan`, `systemdRunArgs`, `matchesAppId` (Tasks 1-2)
- Produces (QML, module `Revive.Shell`):
  - singleton `AppModel`: roles `name`, `icon`, `appId`; `Q_INVOKABLE bool launch(QString appId)`;
    `Q_INVOKABLE bool matches(QString appId, QString waylandAppId)`
  - singleton `Theme`: `background`, `panel`, `accent`, `text`, `dim` (colors), `mono` (font family)
  - image provider `image://icon/<theme name or absolute path>`
  - context property `outputScale` (int, from `REVIVE_SCALE`, default 1)
  - Wayland socket name the apps use: `wayland-0` (`AppModel` and `Main.qml` both use this literal)

- [ ] **Step 1: Write `shell/src/appmodel.h`**

```cpp
#pragma once
#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include "desktopentry.h"

// Installed apps for the launcher, and launching them.
class AppModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    enum Role { NameRole = Qt::UserRole + 1, IconRole, AppIdRole };
    explicit AppModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool launch(const QString &appId);
    Q_INVOKABLE bool matches(const QString &appId, const QString &waylandAppId) const;

private:
    const AppEntry *find(const QString &appId) const;
    QList<AppEntry> m_apps;
};
```

- [ ] **Step 2: Write `shell/src/appmodel.cpp`**

```cpp
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
```

- [ ] **Step 3: Write `shell/src/iconprovider.h` and `shell/src/main.cpp`**

```cpp
#pragma once
#include <QFileInfo>
#include <QIcon>
#include <QQuickImageProvider>

// image://icon/<name>: an icon-theme name, or an absolute path as some .desktop files use.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        const int s = requested.width() > 0 ? requested.width() : 192;
        const QIcon icon = QFileInfo(id).isAbsolute() ? QIcon(id) : QIcon::fromTheme(id);
        const QPixmap pm = icon.pixmap(s, s);
        if (size) *size = pm.size();
        return pm;  // null when missing: QML shows a letter tile instead
    }
};
```
```cpp
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
```

- [ ] **Step 4: Write `shell/qml/Theme.qml` and a temporary `shell/qml/Main.qml`**

```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#050806"
    readonly property color panel: "#0b120d"
    readonly property color accent: "#39ff88"
    readonly property color text: "#c9e8d3"
    readonly property color dim: "#6f8f7a"
    readonly property string mono: "JetBrains Mono"
}
```
Temporary `Main.qml` (Task 4 replaces it), to check the model on a plain window:
```qml
import QtQuick
import QtQuick.Window
import Revive.Shell

Window {
    visible: true
    color: Theme.background
    ListView {
        anchors.fill: parent
        model: AppModel
        delegate: Text { required property string name; text: "> " + name; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 16 }
    }
}
```

- [ ] **Step 5: Extend `shell/CMakeLists.txt`**: replace the `find_package` line and append the executable

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Test)
```
```cmake
qt_add_executable(revive-shell src/main.cpp src/appmodel.cpp src/appmodel.h src/iconprovider.h)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(revive-shell
    URI Revive.Shell
    VERSION 1.0
    QML_FILES qml/Main.qml qml/Theme.qml)
target_link_libraries(revive-shell PRIVATE revive_core Qt6::Gui Qt6::Qml Qt6::Quick)
install(TARGETS revive-shell RUNTIME DESTINATION bin)
```

- [ ] **Step 6: Build on the phone and run it on the screen**

```bash
cd shell && tar -cf - CMakeLists.txt src qml tests | ssh phone@172.16.42.1 'rm -rf ~/revive-shell-src && mkdir -p ~/revive-shell-src && tar -xf - -C ~/revive-shell-src' && ssh phone@172.16.42.1 'cd ~/revive-shell-src && cmake -S . -B build >/dev/null && cmake --build build -j8 2>&1 | tail -2 && ctest --test-dir build | tail -1 && echo 1234 | sudo -S -p "" sh -c "systemctl stop buffyboard; REVIVE_SCALE=3 timeout 10 ./build/revive-shell; systemctl start buffyboard"; echo exit=$?'
```
Expected: tests pass. For about 10 s the screen shows a green list of installed apps (at least `foot` once it's installed in Task 4; before that, whatever `.desktop` files exist). `exit=124` (timeout) means it ran without crashing.

- [ ] **Step 7: Commit**

```bash
git add shell/CMakeLists.txt shell/src/appmodel.h shell/src/appmodel.cpp shell/src/iconprovider.h shell/src/main.cpp shell/qml/Theme.qml shell/qml/Main.qml
git commit -m "shell: AppModel, icon provider and revive-shell executable"
```

---

### Task 4: Compositor UI: status bar, launcher, fullscreen apps, swipe-up home

**Files:**
- Create: `shell/qml/StatusBar.qml`, `shell/qml/Launcher.qml`
- Modify: `shell/qml/Main.qml` (full replacement), `shell/CMakeLists.txt` (QML_FILES)

**Interfaces:**
- Consumes: `AppModel`, `Theme`, `outputScale`, `image://icon/` (Task 3)
- Produces: `Launcher { signal launch(string appId) }`; `StatusBar` (height 28 logical px)

- [ ] **Step 1: Install foot on the phone** (the first app)

```bash
ssh phone@172.16.42.1 'echo 1234 | sudo -S apt-get install -y -q foot fonts-jetbrains-mono hicolor-icon-theme 2>&1 | tail -1; ls /usr/share/applications | grep -i foot'
```
Expected: `foot.desktop` (plus `footclient.desktop`/`foot-server.desktop`, which are fine).

- [ ] **Step 2: Write `shell/qml/StatusBar.qml`**

```qml
import QtQuick
import Revive.Shell

Rectangle {
    id: bar
    height: 28
    color: Theme.panel
    property string now: Qt.formatTime(new Date(), "hh:mm")
    Timer { interval: 1000; running: true; repeat: true; onTriggered: bar.now = Qt.formatTime(new Date(), "hh:mm") }

    Text {
        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
        text: "revive"; color: Theme.accent
        font { family: Theme.mono; pixelSize: 13; bold: true }
    }
    Text {
        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
        text: bar.now; color: Theme.text
        font { family: Theme.mono; pixelSize: 13 }
    }
}
```

- [ ] **Step 3: Write `shell/qml/Launcher.qml`**

```qml
import QtQuick
import Revive.Shell

Item {
    id: root
    signal launch(string appId)

    GridView {
        id: grid
        anchors { fill: parent; margins: 12 }
        cellWidth: width / 4
        cellHeight: cellWidth + 22
        model: AppModel
        delegate: Item {
            id: cell
            required property string name
            required property string icon
            required property string appId
            width: grid.cellWidth; height: grid.cellHeight

            Rectangle {
                id: tile
                width: parent.width - 16; height: width
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 14; color: Theme.panel
                border { color: tap.pressed ? Theme.accent : Theme.dim; width: 1 }
                Image {
                    id: img
                    anchors { fill: parent; margins: 10 }
                    source: cell.icon ? "image://icon/" + cell.icon : ""
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
                Text {  // no icon: terminal-style letter tile
                    anchors.centerIn: parent
                    visible: img.status !== Image.Ready
                    text: cell.name.charAt(0).toUpperCase()
                    color: Theme.accent
                    font { family: Theme.mono; pixelSize: 26; bold: true }
                }
            }
            Text {
                anchors { top: tile.bottom; topMargin: 4; horizontalCenter: parent.horizontalCenter }
                width: parent.width - 4
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: cell.name; color: Theme.text
                font { family: Theme.mono; pixelSize: 11 }
            }
            TapHandler { id: tap; onTapped: root.launch(cell.appId) }
        }
    }
}
```

- [ ] **Step 4: Replace `shell/qml/Main.qml` with the compositor**

```qml
import QtQuick
import QtQuick.Window
import QtWayland.Compositor
import QtWayland.Compositor.XdgShell
import Revive.Shell

WaylandCompositor {
    id: comp
    socketName: "wayland-0"  // AppModel.launch() sets the same WAYLAND_DISPLAY

    property var current: null  // XdgSurface shown fullscreen; null = home (launcher)

    function findSurface(appId) {
        for (let i = 0; i < surfaces.count; ++i) {
            const s = surfaces.get(i).shellSurface
            if (s.toplevel && AppModel.matches(appId, s.toplevel.appId)) return s
        }
        return null
    }
    function removeSurface(s) {
        for (let i = 0; i < surfaces.count; ++i)
            if (surfaces.get(i).shellSurface === s) { surfaces.remove(i); break }
        if (current === s) current = null  // the app closed or crashed: back home, never a black screen
    }

    ListModel { id: surfaces }

    WaylandOutput {
        sizeFollowsWindow: true
        scaleFactor: outputScale
        window: Window {
            id: win
            visible: true
            visibility: Window.FullScreen
            color: Theme.background

            StatusBar { id: bar; anchors { top: parent.top; left: parent.left; right: parent.right } }

            Launcher {
                anchors { top: bar.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
                visible: comp.current === null
                onLaunch: (appId) => {
                    const running = comp.findSurface(appId)
                    if (running) comp.current = running
                    else AppModel.launch(appId)
                }
            }

            Repeater {
                model: surfaces
                ShellSurfaceItem {
                    shellSurface: modelData
                    anchors { top: bar.bottom; left: parent.left }
                    visible: comp.current === modelData
                    autoCreatePopupItems: true
                    onVisibleChanged: if (visible) takeFocus()
                    onSurfaceDestroyed: comp.removeSurface(modelData)
                }
            }

            // Swipe up from the bottom edge: home. The app keeps running.
            Item {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 28; z: 10
                visible: comp.current !== null
                Rectangle { anchors.centerIn: parent; width: 120; height: 5; radius: 3; color: Theme.dim }
                DragHandler {
                    id: swipe
                    target: null
                    xAxis.enabled: false
                    onActiveChanged: if (!active && swipe.translation.y < -60) comp.current = null
                }
            }
        }
    }

    XdgShell {
        onToplevelCreated: (toplevel, xdgSurface) => {
            toplevel.sendMaximized(Qt.size(win.width, win.height - bar.height))
            surfaces.append({ shellSurface: xdgSurface })
            comp.current = xdgSurface  // a new window is what the user just opened
        }
    }
}
```
In `shell/CMakeLists.txt` set `QML_FILES qml/Main.qml qml/Theme.qml qml/StatusBar.qml qml/Launcher.qml`.

- [ ] **Step 5: Build and run on the phone; exercise the Review Focus checks by hand**

The shell needs a real logind session for GPU/input access and for `systemd-run --user`. Run it as a
transient unit with the same session settings Task 5's service uses. `dbus-user-session` provides the
user bus for `systemd-run --user`.
```bash
cd shell && tar -cf - CMakeLists.txt src qml tests | ssh phone@172.16.42.1 'rm -rf ~/revive-shell-src && mkdir -p ~/revive-shell-src && tar -xf - -C ~/revive-shell-src' && ssh phone@172.16.42.1 'cd ~/revive-shell-src && cmake -S . -B build >/dev/null && cmake --build build -j8 2>&1 | tail -1 && ctest --test-dir build | tail -1 && echo 1234 | sudo -S -p "" sh -c "apt-get install -y -q dbus-user-session >/dev/null; systemctl stop buffyboard getty@tty1; systemd-run --unit=revive-shell-dev -p User=phone -p PAMName=login -p TTYPath=/dev/tty1 -p StandardInput=tty-fail -p Environment=REVIVE_SCALE=3 /home/phone/revive-shell-src/build/revive-shell"; sleep 6; systemctl is-active revive-shell-dev; journalctl -u revive-shell-dev --no-pager | tail -3'
```
Expected, checked with the user on the phone:
1. Green `revive` bar with clock, app grid underneath, foot among the apps.
2. Tap foot: a terminal fills the screen below the bar, and typing works over SSH-forwarded input.
3. Swipe up from the bottom bar: launcher. Tap foot again: the **same** terminal comes back, with no second copy (`ssh phone@172.16.42.1 'pgrep -c foot'` prints `1`).
4. In foot run `exit`: back to the launcher, not black.
Then stop it: `ssh phone@172.16.42.1 'echo 1234 | sudo -S systemctl stop revive-shell-dev; echo 1234 | sudo -S systemctl start getty@tty1 buffyboard'`.

- [ ] **Step 6: Commit**

```bash
git add shell/qml/Main.qml shell/qml/StatusBar.qml shell/qml/Launcher.qml shell/CMakeLists.txt
git commit -m "shell: compositor with status bar, launcher, fullscreen apps and swipe-up home"
```

---

### Task 5: Boot into the shell (session service) with console fallback

**Files:**
- Create: `rootfs/etc/systemd/system/revive-shell.service`

**Interfaces:**
- Consumes: `/usr/bin/revive-shell` (Tasks 3-4), `/etc/revive/shell.env` with `REVIVE_SCALE=3` (written by hand now, by build.sh in Task 6)
- Produces: `revive-shell.service`: on boot, a logind session for `phone` on tty1 runs the shell. After 3 failures within 60 s, getty and buffyboard take tty1 back.

- [ ] **Step 1: Write `rootfs/etc/systemd/system/revive-shell.service`**

```ini
[Unit]
Description=revive shell
After=systemd-user-sessions.service systemd-logind.service
Conflicts=getty@tty1.service buffyboard.service
# Never leave a dead screen: after repeated crashes give tty1 back to the text console.
OnFailure=getty@tty1.service buffyboard.service
StartLimitIntervalSec=60
StartLimitBurst=3

[Service]
User=phone
# A real logind session: GPU/input access, /run/user/1000 and systemd --user for apps.
PAMName=login
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes
UtmpIdentifier=tty1
UtmpMode=user
StandardInput=tty-fail
StandardOutput=journal
StandardError=journal
EnvironmentFile=-/etc/revive/shell.env
Environment=XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=revive
ExecStart=/usr/bin/revive-shell
Restart=on-failure
RestartSec=1

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 2: Install it on the phone with the shell, switch tty1 over, reboot**

```bash
scp rootfs/etc/systemd/system/revive-shell.service phone@172.16.42.1:/tmp/ && ssh phone@172.16.42.1 'P=1234; s(){ echo $P | sudo -S -p "" "$@"; }
s cmake --install ~/revive-shell-src/build --prefix /usr
s install -D -m 644 /tmp/revive-shell.service /etc/systemd/system/revive-shell.service
s mkdir -p /etc/revive; echo REVIVE_SCALE=3 | s tee /etc/revive/shell.env >/dev/null
s systemctl disable buffyboard getty@tty1; s systemctl enable revive-shell; s systemctl reboot'
```
- [ ] **Step 3: Verify after the reboot**

Wait for SSH, then:
```bash
ssh phone@172.16.42.1 'systemctl is-active revive-shell; loginctl list-sessions --no-legend; journalctl -b -u revive-shell --no-pager | tail -5; systemctl --user -M phone@ list-units "app-revive-*" --no-legend 2>/dev/null | head'
```
Expected: `active`; a session for `phone` on `tty1`; no errors in the journal. On the phone: the shell is up without anyone logging in. Launch foot from the grid, then the `systemctl --user` line lists an `app-revive-foot-*.scope`.

- [ ] **Step 4: Verify the fallback**

```bash
ssh phone@172.16.42.1 'echo 1234 | sudo -S -p "" sh -c "for i in 1 2 3 4; do pkill -SEGV -x revive-shell; sleep 3; done"; sleep 3; systemctl is-active revive-shell getty@tty1 buffyboard | paste -sd" "'
```
Expected: `failed active active`, and the phone shows the text login with the on-screen keyboard. Restore with
`ssh phone@172.16.42.1 'echo 1234 | sudo -S systemctl reset-failed revive-shell; echo 1234 | sudo -S systemctl restart revive-shell'`.

- [ ] **Step 5: Commit**

```bash
git add rootfs/etc/systemd/system/revive-shell.service
git commit -m "Boot into revive-shell via a logind session; fall back to the text console on repeated failure"
```

---

### Task 6: Image integration: build.sh builds the shell; GPU firmware, packages and session in the image

**Files:**
- Modify: `build.sh`, `Dockerfile`, `devices/xiaomi-jasmine/device.conf`

**Interfaces:**
- Consumes: `shell/` (Tasks 1-4), `revive-shell.service` (Task 5)
- Produces: `./build.sh xiaomi-jasmine` images that boot straight into revive-shell with the GPU working.

- [ ] **Step 1: `Dockerfile`: let apt in the build container fetch `non-free-firmware` packages** (architecture `all`)

Add after the existing `RUN` line:
```dockerfile
RUN sed -i 's/^Components: main$/Components: main non-free-firmware/' /etc/apt/sources.list.d/debian.sources && apt-get update
```

- [ ] **Step 2: `devices/xiaomi-jasmine/device.conf`: scale, GPU microcode, zap shader**

Append:
```sh
# Shell: integer wl_output scale (1080 px / 3 = 360 logical px).
SCALE=3
# Adreno 5xx microcode: two files from Debian's 115 MB firmware-qcom-soc instead of the whole package.
FIRMWARE_FROM_DEB="firmware-qcom-soc:usr/lib/firmware/qcom/a530_pm4.fw,usr/lib/firmware/qcom/a530_pfp.fw"
```
(The zap shader `a512_zap.mbn` is already in the postmarketOS firmware package, under `postmarketos/`. Step 3 moves it to the firmware root, where the device tree looks for it.)

- [ ] **Step 3: `build.sh`: packages, firmware, shell build, session**

3a. In the `mmdebstrap` call, add `--components=main,non-free-firmware` and extend `--include=` with:
`systemd-timesyncd,dbus-user-session,foot,fonts-jetbrains-mono,hicolor-icon-theme,libgl1-mesa-dri,libegl1,qt6-qpa-plugins,qt6-wayland,qml6-module-qtquick,qml6-module-qtquick-window,qml6-module-qtwayland-compositor`.
In the chroot `systemctl enable` hook, replace `buffyboard` with `revive-shell` and add `systemctl disable getty@tty1`.
buffyboard and getty stay installed as the fallback.

3b. In the `FIRMWARE_URL` block, change the `tar` line to:
```sh
  tar -xzif "$W/firmware.tar.gz" -C "$R" --no-same-owner --wildcards 'usr/lib/firmware/*' \
    --transform 's,^usr/lib/firmware/postmarketos/,usr/lib/firmware/,' 2>/dev/null
```
Then add after the block:
```sh
# Single files out of large firmware .debs (FIRMWARE_FROM_DEB="pkg:file,file ...").
for spec in ${FIRMWARE_FROM_DEB:-}; do
  pkg=${spec%%:*}
  (cd "$W" && rm -rf fwx "${pkg}"_*.deb && apt-get download -q "$pkg" && dpkg-deb -x "${pkg}"_*.deb fwx)
  for f in $(echo "${spec#*:}" | tr ',' ' '); do install -D -m 644 "$W/fwx/$f" "$R/$f"; done
done
```

3c. After the firmware blocks, build the shell in a cached arm64 SDK chroot and install it:
```sh
echo "== revive-shell"
B=/work/sdk-forky
[ -d "$B" ] || mmdebstrap --mode=root --arch=arm64 --variant=apt \
  --include=build-essential,cmake,qt6-base-dev,qt6-declarative-dev forky "$B" http://deb.debian.org/debian
cp /etc/resolv.conf "$B/etc/resolv.conf"
mount -t proc proc "$B/proc"; trap 'umount "$B/proc" 2>/dev/null || true' EXIT
chroot "$B" sh -ec 'apt-get -q update && apt-get -qy dist-upgrade >/dev/null'  # same Qt as the rootfs
rm -rf "$B/src" "$B/out"; cp -r "$SRC/shell" "$B/src"
chroot "$B" sh -ec 'cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build /src/build -j"$(nproc)"
  ctest --test-dir /src/build --output-on-failure
  DESTDIR=/out cmake --install /src/build --prefix /usr'
umount "$B/proc"; trap - EXIT
cp -a "$B/out/." "$R/"
mkdir -p "$R/etc/revive"; echo "REVIVE_SCALE=${SCALE:-1}" > "$R/etc/revive/shell.env"
```

- [ ] **Step 4: Full build**

Run: `./build.sh xiaomi-jasmine`
Expected: the `== revive-shell` section prints `100% tests passed`, and the build ends with the `ls -lh` of the three images.
Then check the image:
```bash
export MSYS_NO_PATHCONV=1; docker run --rm -v "D:/Dev/LinuxPhone/out:/o" linuxphone-build bash -c 'simg2img /o/xiaomi-jasmine/rootfs.img /tmp/r && for f in /usr/bin/revive-shell /usr/lib/firmware/qcom/a530_pm4.fw /usr/lib/firmware/a512_zap.mbn /usr/lib/firmware/ath10k/WCN3990/hw1.0/board-2.bin /etc/revive/shell.env /etc/systemd/system/multi-user.target.wants/revive-shell.service; do printf "%s " $f; debugfs -R "stat $f" /tmp/r 2>/dev/null | grep -q Inode && echo ok || echo MISSING; done'
```
Expected: every line `ok`.

- [ ] **Step 5: Flash and verify on the phone** (with the user: phone in fastboot)

```bash
adb kill-server; fastboot flash userdata out/xiaomi-jasmine/rootfs.img
```
The user holds Power to reboot. Expected: logo, then revive-shell **with no login**. foot launches. `ssh phone@172.16.42.1 'systemctl is-active revive-shell qcom-firmware iwd'` prints `active` three times. Wi-Fi reconnects only after `iwctl` is given the password again, because a reflash wipes `/var/lib/iwd`.

- [ ] **Step 6: Commit and push**

```bash
git add build.sh Dockerfile devices/xiaomi-jasmine/device.conf
git commit -m "Image boots into revive-shell: shell built in an arm64 SDK chroot, GPU firmware, session packages"
git push origin main
```

---

### Task 7: Docs

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-11-28-revive-shell-and-sdk-design.md`, `CONTRIBUTING.md`

- [ ] **Step 1: README:** status row adds `shell 3a: launcher + apps`. Under "Use", add: *"The phone boots into revive-shell. Swipe up from the bottom edge to go home. If the shell keeps crashing, tty1 falls back to the text console."* Add `shell/` to the Layout block: `shell/  revive-shell (Qt Wayland compositor + QML UI); ./shell/dev-deploy.sh builds it on a phone`.
- [ ] **Step 2: Spec:** change "scale 2.5 gives a 432×864 logical screen" to "scale 3 gives a 360×720 logical screen (`wl_output` scale is an integer)".
- [ ] **Step 3: CONTRIBUTING:** add `SCALE` and `FIRMWARE_FROM_DEB` to the optional `device.conf` keys.
- [ ] **Step 4: Commit and push**

```bash
git add README.md CONTRIBUTING.md docs/superpowers/specs/2026-11-28-revive-shell-and-sdk-design.md
git commit -m "Docs: revive-shell 3a, device SCALE and FIRMWARE_FROM_DEB"
git push origin main
```
