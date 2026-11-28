#pragma once
#include <QString>

// Battery state read from a sysfs power_supply tree.
struct Battery {
    int percent = -1;        // 0-100, or -1 when unknown
    bool charging = false;   // true only for status "Charging"
};

// First entry under powerSupplyRoot whose "type" is "Battery": its "capacity" and "status".
// Missing or unreadable -> percent -1.
Battery readBattery(const QString &powerSupplyRoot);

// Wi-Fi signal strength in dBm as 0-4 bars.
int signalBars(int dBm);
