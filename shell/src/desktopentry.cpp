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
