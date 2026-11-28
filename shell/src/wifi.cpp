#include "wifi.h"
#include <algorithm>

QList<Network> sortNetworks(QList<Network> networks) {
    std::stable_sort(networks.begin(), networks.end(), [](const Network &a, const Network &b) {
        if (a.connected != b.connected)
            return a.connected;
        if (a.signal100dBm != b.signal100dBm)
            return a.signal100dBm > b.signal100dBm;
        return a.name < b.name;
    });
    return networks;
}

bool isSecured(const QString &type) {
    return type != QLatin1String("open");
}
