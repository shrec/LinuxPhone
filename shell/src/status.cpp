#include "status.h"
#include <QDir>
#include <QFile>

static QString readTrimmed(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

Battery readBattery(const QString &powerSupplyRoot) {
    Battery b;
    const QDir root(powerSupplyRoot);
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : entries) {
        const QString dir = root.filePath(name);
        if (readTrimmed(dir + "/type") != "Battery")
            continue;
        bool ok = false;
        const int capacity = readTrimmed(dir + "/capacity").toInt(&ok);
        if (ok && capacity >= 0 && capacity <= 100) {
            b.percent = capacity;
            b.charging = readTrimmed(dir + "/status") == "Charging";
        }
        return b;
    }
    return b;
}

int signalBars(int dBm) {
    if (dBm >= -55) return 4;
    if (dBm >= -66) return 3;
    if (dBm >= -77) return 2;
    if (dBm >= -88) return 1;
    return 0;
}
