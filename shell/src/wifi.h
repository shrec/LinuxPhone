#pragma once
#include <QList>
#include <QString>

// One Wi-Fi network as iwd reports it (Station.GetOrderedNetworks plus the Network properties).
struct Network {
    QString path;            // iwd object path of the Network
    QString name;            // SSID
    QString type;            // "open", "psk", "8021x", "wep"
    int signal100dBm = 0;    // 100 * dBm, as iwd reports it (-2700 = -27 dBm)
    bool known = false;      // a KnownNetwork (saved profile) exists
    bool connected = false;
};

// Connected first, then the strongest signal, then by name. Stable for equal entries.
QList<Network> sortNetworks(QList<Network> networks);

// Everything except an "open" network needs credentials.
bool isSecured(const QString &type);
