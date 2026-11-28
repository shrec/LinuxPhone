#include "wifimodel.h"
#include "status.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QMap>
#include <QSet>
#include <QVariantMap>

static const int kPollIntervalMs = 10000;
static const int kDebounceMs = 150;
static const int kCallTimeoutMs = 5000;
static const int kConnectTimeoutMs = 60000;

static const char kIwdService[] = "net.connman.iwd";
static const char kIwdRoot[] = "/net/connman/iwd";
static const char kAgentPath[] = "/revive/iwd_agent";
static const char kAgentManagerIface[] = "net.connman.iwd.AgentManager";
static const char kDeviceIface[] = "net.connman.iwd.Device";
static const char kStationIface[] = "net.connman.iwd.Station";
static const char kNetworkIface[] = "net.connman.iwd.Network";
static const char kKnownNetworkIface[] = "net.connman.iwd.KnownNetwork";
static const char kPropertiesIface[] = "org.freedesktop.DBus.Properties";
static const char kObjectManagerIface[] = "org.freedesktop.DBus.ObjectManager";
static const char kCanceledError[] = "net.connman.iwd.Agent.Error.Canceled";

// "net.connman.iwd.Failed" -> "Failed": short enough for the panel.
static QString shortError(const QDBusMessage &reply) {
    const QString name = reply.errorName().section('.', -1);
    return name.isEmpty() ? QStringLiteral("failed") : name;
}

// Not worth showing: iwd is not running (yet), or the agent is already registered.
static bool isBenign(const QDBusMessage &reply) {
    const QString name = reply.errorName().section('.', -1);
    return name == QLatin1String("ServiceUnknown") || name == QLatin1String("NameHasNoOwner") ||
           name == QLatin1String("AlreadyExists");
}

static bool isRealPath(const QString &path) {
    return !path.isEmpty() && path != QLatin1String("/");
}

template <typename T>
static bool assign(T &field, const T &value) {
    if (field == value)
        return false;
    field = value;
    return true;
}

static bool sameNetwork(const Network &a, const Network &b) {
    return a.path == b.path && a.name == b.name && a.type == b.type && a.signal100dBm == b.signal100dBm &&
           a.known == b.known && a.connected == b.connected;
}

// ---- agent ----

void WifiAgent::setPassphrase(const QString &networkPath, const QString &passphrase) {
    m_passphrases.insert(networkPath, passphrase);
}

void WifiAgent::clearPassphrase(const QString &networkPath) {
    m_passphrases.remove(networkPath);
}

void WifiAgent::Release() {
    m_passphrases.clear();
}

QString WifiAgent::RequestPassphrase(const QDBusObjectPath &network) {
    const auto it = m_passphrases.constFind(network.path());
    if (it == m_passphrases.constEnd()) {
        sendErrorReply(QString::fromLatin1(kCanceledError), QStringLiteral("no passphrase"));
        return QString();
    }
    return it.value();
}

QString WifiAgent::RequestPrivateKeyPassphrase(const QDBusObjectPath &) {
    sendErrorReply(QString::fromLatin1(kCanceledError), QStringLiteral("not supported"));
    return QString();
}

QString WifiAgent::RequestUserNameAndPassword(const QDBusObjectPath &, QString &password) {
    password.clear();
    sendErrorReply(QString::fromLatin1(kCanceledError), QStringLiteral("not supported"));
    return QString();
}

void WifiAgent::Cancel(const QString &) {
    m_passphrases.clear();
}

// ---- model ----

WifiModel::WifiModel(QObject *parent) : QAbstractListModel(parent) {
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &WifiModel::refresh);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &WifiModel::refresh);

    QDBusConnection bus = QDBusConnection::systemBus();
    m_agent = new WifiAgent(this);
    if (bus.isConnected()) {
        bus.registerObject(QString::fromLatin1(kAgentPath), m_agent, QDBusConnection::ExportAllSlots);
        bus.connect(QString::fromLatin1(kIwdService), QString(), QString::fromLatin1(kPropertiesIface),
                    QStringLiteral("PropertiesChanged"), this, SLOT(onIwdSignal(QDBusMessage)));
        bus.connect(QString::fromLatin1(kIwdService), QString(), QString::fromLatin1(kObjectManagerIface),
                    QStringLiteral("InterfacesAdded"), this, SLOT(onIwdSignal(QDBusMessage)));
        bus.connect(QString::fromLatin1(kIwdService), QString(), QString::fromLatin1(kObjectManagerIface),
                    QStringLiteral("InterfacesRemoved"), this, SLOT(onIwdSignal(QDBusMessage)));

        // iwd (re)appears on the bus: its agent registration is gone, register again.
        m_watcher = new QDBusServiceWatcher(QString::fromLatin1(kIwdService), bus,
                                            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                [this](const QString &, const QString &, const QString &newOwner) {
                    if (newOwner.isEmpty()) {
                        applySnapshot(Snapshot(), QList<Network>());
                        return;
                    }
                    registerAgent();
                    refresh();
                });
    }
    registerAgent();
    refresh();
}

int WifiModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_networks.size());
}

QVariant WifiModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_networks.size())
        return QVariant();
    const Network &n = m_networks.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return n.name;
    case PathRole: return n.path;
    case BarsRole: return signalBars(n.signal100dBm / 100);
    case SecuredRole: return isSecured(n.type);
    case KnownRole: return n.known;
    case ConnectedRole: return n.connected;
    default: return QVariant();
    }
}

QHash<int, QByteArray> WifiModel::roleNames() const {
    return {{NameRole, "name"},
            {PathRole, "path"},
            {BarsRole, "bars"},
            {SecuredRole, "secured"},
            {KnownRole, "known"},
            {ConnectedRole, "connected"}};
}

void WifiModel::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    if (active) {
        m_pollTimer.start();
        refresh();
    } else {
        m_pollTimer.stop();
    }
    emit activeChanged();
}

void WifiModel::setLastError(const QString &error) {
    if (assign(m_lastError, error))
        emit lastErrorChanged();
}

void WifiModel::onIwdSignal(const QDBusMessage &) {
    m_debounce.start();  // coalesce bursts of PropertiesChanged
}

// One asynchronous call; a failure goes to lastError, then done(ok) runs either way.
void WifiModel::asyncCall(const QDBusMessage &message, int timeoutMs, const QString &what,
                          std::function<void(bool)> done) {
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        setLastError(what + QStringLiteral(": no system bus"));
        if (done)
            done(false);
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, what, done](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                const bool ok = reply.type() == QDBusMessage::ReplyMessage;
                if (!ok && !isBenign(reply))
                    setLastError(what + QStringLiteral(": ") + shortError(reply));
                if (done)
                    done(ok);
            });
}

void WifiModel::registerAgent() {
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), QString::fromLatin1(kIwdRoot),
                                                      QString::fromLatin1(kAgentManagerIface),
                                                      QStringLiteral("RegisterAgent"));
    msg << QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(kAgentPath)));
    asyncCall(msg, kCallTimeoutMs, QStringLiteral("agent"), nullptr);
}

void WifiModel::refresh() {
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        applySnapshot(Snapshot(), QList<Network>());
        return;
    }
    if (m_refreshBusy) {
        m_refreshAgain = true;
        return;
    }
    m_refreshBusy = true;
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), QStringLiteral("/"),
                                                      QString::fromLatin1(kObjectManagerIface),
                                                      QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg, kCallTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        onManagedObjects(w->reply());
    });
}

void WifiModel::finishRefresh() {
    m_refreshBusy = false;
    if (m_refreshAgain) {
        m_refreshAgain = false;
        m_debounce.start();
    }
}

// First step of a refresh: device, station and every network's properties.
void WifiModel::onManagedObjects(const QDBusMessage &reply) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (reply.type() == QDBusMessage::ErrorMessage && !isBenign(reply))
            setLastError(QStringLiteral("wifi: ") + shortError(reply));
        applySnapshot(Snapshot(), QList<Network>());
        finishRefresh();
        return;
    }
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::MapType) {
        applySnapshot(Snapshot(), QList<Network>());
        finishRefresh();
        return;
    }

    QMap<QString, bool> devices;           // device path -> Powered
    QMap<QString, QVariantMap> stations;   // station path -> properties
    QHash<QString, NetInfo> nets;

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
            if (iface == QLatin1String(kDeviceIface)) {
                devices.insert(path.path(), props.value("Powered").toBool());
            } else if (iface == QLatin1String(kStationIface)) {
                stations.insert(path.path(), props);
            } else if (iface == QLatin1String(kNetworkIface)) {
                NetInfo info;
                info.name = props.value("Name").toString();
                info.type = props.value("Type").toString();
                info.connected = props.value("Connected").toBool();
                info.knownPath = qvariant_cast<QDBusObjectPath>(props.value("KnownNetwork")).path();
                nets.insert(path.path(), info);
            }
        }
        arg.endMap();
        arg.endMapEntry();
    }
    arg.endMap();

    Snapshot snap;
    snap.nets = nets;
    if (!stations.isEmpty()) {
        snap.stationPath = stations.firstKey();
        snap.devicePath = snap.stationPath;
        snap.powered = devices.value(snap.devicePath, true);
        const QVariantMap &props = stations.first();
        snap.scanning = props.value("Scanning").toBool();
        snap.state = props.value("State").toString();
        snap.connectedNetwork = qvariant_cast<QDBusObjectPath>(props.value("ConnectedNetwork")).path();
    } else if (!devices.isEmpty()) {
        snap.devicePath = devices.firstKey();
        snap.powered = devices.first();
    }

    if (snap.stationPath.isEmpty()) {
        applySnapshot(snap, QList<Network>());
        finishRefresh();
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), snap.stationPath,
                                                      QString::fromLatin1(kStationIface),
                                                      QStringLiteral("GetOrderedNetworks"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg, kCallTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, snap](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        onOrderedNetworks(snap, w->reply());
    });
}

// Second step: the visible networks with their signal, a(on) with the signal in 100 * dBm.
void WifiModel::onOrderedNetworks(const Snapshot &snap, const QDBusMessage &reply) {
    QList<Network> networks;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (reply.type() == QDBusMessage::ErrorMessage && !isBenign(reply))
            setLastError(QStringLiteral("wifi: ") + shortError(reply));
        applySnapshot(snap, networks);
        finishRefresh();
        return;
    }
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    if (arg.currentType() == QDBusArgument::ArrayType) {
        arg.beginArray();
        while (!arg.atEnd()) {
            QDBusObjectPath path;
            qint16 strength = 0;
            arg.beginStructure();
            arg >> path >> strength;
            arg.endStructure();
            const auto it = snap.nets.constFind(path.path());
            if (it == snap.nets.constEnd() || it->name.isEmpty())
                continue;
            Network n;
            n.path = path.path();
            n.name = it->name;
            n.type = it->type;
            n.signal100dBm = strength;
            n.known = isRealPath(it->knownPath);
            n.connected = it->connected;
            networks.append(n);
        }
        arg.endArray();
    }
    applySnapshot(snap, networks);
    finishRefresh();
}

void WifiModel::applySnapshot(const Snapshot &snap, const QList<Network> &networks) {
    m_devicePath = snap.devicePath;
    m_stationPath = snap.stationPath;

    // A powered-off device has no Station interface; it is still there to be switched on.
    const bool available = !snap.devicePath.isEmpty() && (!snap.stationPath.isEmpty() || !snap.powered);
    const QList<Network> sorted = sortNetworks(networks);

    QString connectedName;
    for (const Network &n : sorted) {
        if (n.connected) {
            connectedName = n.name;
            break;
        }
    }
    if (connectedName.isEmpty() && isRealPath(snap.connectedNetwork))
        connectedName = snap.nets.value(snap.connectedNetwork).name;

    m_knownPaths.clear();
    for (const Network &n : sorted) {
        const QString known = snap.nets.value(n.path).knownPath;
        if (isRealPath(known))
            m_knownPaths.insert(n.path, known);
    }

    if (assign(m_available, available))
        emit availableChanged();
    if (assign(m_powered, snap.powered && !snap.devicePath.isEmpty()))
        emit poweredChanged();
    if (assign(m_scanning, snap.scanning))
        emit scanningChanged();
    if (assign(m_state, snap.state))
        emit stateChanged();
    if (assign(m_connectedName, connectedName))
        emit connectedNameChanged();
    setNetworks(sorted);
}

// A new or vanished network, a change of the connected one, or a different sorted order rebuilds the
// list. When the order is the same only the row values change, so the list does not jump while the
// user is tapping or typing a passphrase.
void WifiModel::setNetworks(const QList<Network> &networks) {
    bool sameOrder = m_networks.size() == networks.size();
    for (qsizetype i = 0; sameOrder && i < networks.size(); ++i) {
        if (m_networks.at(i).path != networks.at(i).path || m_networks.at(i).connected != networks.at(i).connected)
            sameOrder = false;
    }

    if (!sameOrder) {
        beginResetModel();
        m_networks = networks;
        endResetModel();
        return;
    }

    // Same rows in the same order: the new list is already index-aligned with the old one.
    bool changed = false;
    for (qsizetype i = 0; i < networks.size(); ++i) {
        if (!sameNetwork(m_networks.at(i), networks.at(i))) {
            changed = true;
            break;
        }
    }
    const QList<Network> &merged = networks;
    if (changed) {
        m_networks = merged;
        emit dataChanged(index(0), index(static_cast<int>(m_networks.size()) - 1));
    }
}

void WifiModel::scan() {
    if (m_stationPath.isEmpty()) {
        setLastError(QStringLiteral("scan: wifi unavailable"));
        return;
    }
    setLastError(QString());
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), m_stationPath,
                                                      QString::fromLatin1(kStationIface), QStringLiteral("Scan"));
    asyncCall(msg, kCallTimeoutMs, QStringLiteral("scan"), [this](bool) { m_debounce.start(); });
}

void WifiModel::setPowered(bool on) {
    if (m_devicePath.isEmpty()) {
        setLastError(QStringLiteral("power: wifi unavailable"));
        return;
    }
    setLastError(QString());
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), m_devicePath,
                                                      QString::fromLatin1(kPropertiesIface), QStringLiteral("Set"));
    msg << QString::fromLatin1(kDeviceIface) << QStringLiteral("Powered") << QVariant::fromValue(QDBusVariant(on));
    asyncCall(msg, kCallTimeoutMs, QStringLiteral("power"), [this](bool) { m_debounce.start(); });
}

void WifiModel::connectTo(const QString &path, const QString &passphrase) {
    QString name;
    for (const Network &n : std::as_const(m_networks)) {
        if (n.path == path) {
            name = n.name;
            break;
        }
    }
    if (name.isEmpty()) {
        setLastError(QStringLiteral("connect: unknown network"));
        return;
    }
    if (!m_connectingName.isEmpty()) {
        setLastError(QStringLiteral("connect: busy"));
        return;
    }
    setLastError(QString());
    if (passphrase.isEmpty())
        m_agent->clearPassphrase(path);
    else
        m_agent->setPassphrase(path, passphrase);
    if (assign(m_connectingName, name))
        emit connectingNameChanged();

    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), path,
                                                      QString::fromLatin1(kNetworkIface), QStringLiteral("Connect"));
    asyncCall(msg, kConnectTimeoutMs, QStringLiteral("connect"), [this, path](bool) {
        m_agent->clearPassphrase(path);
        if (assign(m_connectingName, QString()))
            emit connectingNameChanged();
        m_debounce.start();
    });
}

void WifiModel::forget(const QString &path) {
    const QString known = m_knownPaths.value(path);
    if (known.isEmpty()) {
        setLastError(QStringLiteral("forget: not a saved network"));
        return;
    }
    setLastError(QString());
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kIwdService), known,
                                                      QString::fromLatin1(kKnownNetworkIface),
                                                      QStringLiteral("Forget"));
    asyncCall(msg, kCallTimeoutMs, QStringLiteral("forget"), [this](bool) { m_debounce.start(); });
}
