#include "statusmodel.h"
#include "status.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

static const char kPowerSupplyRoot[] = "/sys/class/power_supply";
static const int kBatteryIntervalMs = 30000;
static const int kWifiIntervalMs = 10000;
static const int kCallTimeoutMs = 5000;

static const char kIwdService[] = "net.connman.iwd";
static const char kStationIface[] = "net.connman.iwd.Station";
static const char kNetworkIface[] = "net.connman.iwd.Network";

StatusModel::StatusModel(QObject *parent) : QObject(parent) {
    m_batteryTimer.setInterval(kBatteryIntervalMs);
    connect(&m_batteryTimer, &QTimer::timeout, this, &StatusModel::pollBattery);
    m_batteryTimer.start();
    pollBattery();

    m_wifiTimer.setInterval(kWifiIntervalMs);
    connect(&m_wifiTimer, &QTimer::timeout, this, &StatusModel::pollWifi);
    m_wifiTimer.start();
    pollWifi();
}

void StatusModel::pollBattery() {
    const Battery b = readBattery(kPowerSupplyRoot);
    if (b.percent != m_percent || b.charging != m_charging) {
        m_percent = b.percent;
        m_charging = b.charging;
        emit batteryChanged();
    }
}

void StatusModel::setWifi(bool connected, const QString &name, int bars) {
    m_wifiBusy = false;
    if (connected == m_wifiConnected && name == m_wifiName && bars == m_wifiBars)
        return;
    m_wifiConnected = connected;
    m_wifiName = name;
    m_wifiBars = bars;
    emit wifiChanged();
}

void StatusModel::pollWifi() {
    if (m_wifiBusy)
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        setWifi(false, QString(), 0);
        return;
    }
    m_wifiBusy = true;
    QDBusMessage msg = QDBusMessage::createMethodCall(kIwdService, "/", "org.freedesktop.DBus.ObjectManager",
                                                      "GetManagedObjects");
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg, kCallTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            setWifi(false, QString(), 0);
            return;
        }
        const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
        if (arg.currentType() != QDBusArgument::MapType) {
            setWifi(false, QString(), 0);
            return;
        }

        QString stationPath;
        QString connectedNetwork;
        bool connected = false;
        QMap<QString, QString> networkNames;  // network object path -> Name

        arg.beginMap();
        while (!arg.atEnd()) {
            QDBusObjectPath path;
            arg.beginMapEntry();
            arg >> path;
            arg.beginMap();
            while (!arg.atEnd()) {
                QString iface;
                QVariantMap props;
                arg.beginMapEntry();
                arg >> iface >> props;
                arg.endMapEntry();
                if (iface == kStationIface && stationPath.isEmpty()) {
                    stationPath = path.path();
                    connected = props.value("State").toString() == "connected";
                    connectedNetwork = qvariant_cast<QDBusObjectPath>(props.value("ConnectedNetwork")).path();
                } else if (iface == kNetworkIface) {
                    networkNames.insert(path.path(), props.value("Name").toString());
                }
            }
            arg.endMap();
            arg.endMapEntry();
        }
        arg.endMap();

        if (stationPath.isEmpty() || !connected || connectedNetwork.isEmpty() || connectedNetwork == "/") {
            setWifi(false, QString(), 0);
            return;
        }
        onManagedObjects(stationPath, connectedNetwork, networkNames.value(connectedNetwork));
    });
}

// Second step of a poll: the signal of the connected network, from Station.GetOrderedNetworks().
void StatusModel::onManagedObjects(const QString &stationPath, const QString &networkPath, const QString &name) {
    QDBusMessage msg = QDBusMessage::createMethodCall(kIwdService, stationPath, kStationIface, "GetOrderedNetworks");
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg, kCallTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, networkPath, name](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            setWifi(false, QString(), 0);
            return;
        }
        const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
        if (arg.currentType() != QDBusArgument::ArrayType) {
            setWifi(false, QString(), 0);
            return;
        }
        int bars = 0;
        arg.beginArray();
        while (!arg.atEnd()) {
            QDBusObjectPath path;
            qint16 strength = 0;  // 100 * dBm
            arg.beginStructure();
            arg >> path >> strength;
            arg.endStructure();
            if (path.path() == networkPath) {
                bars = signalBars(strength / 100);
                break;
            }
        }
        // No endArray() after an early break: the QDBusArgument copy is discarded anyway.
        setWifi(true, name, bars);
    });
}
