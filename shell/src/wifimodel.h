#pragma once
#include "wifi.h"
#include <QAbstractListModel>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QList>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <functional>

// The iwd agent: iwd calls it while connecting to a secured network that has no saved profile.
// It answers with the passphrase WifiModel::connectTo stored for that network, or with Canceled.
class WifiAgent : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.Agent")
public:
    explicit WifiAgent(QObject *parent = nullptr) : QObject(parent) {}
    void setPassphrase(const QString &networkPath, const QString &passphrase);
    void clearPassphrase(const QString &networkPath);

public slots:
    void Release();
    QString RequestPassphrase(const QDBusObjectPath &network);
    QString RequestPrivateKeyPassphrase(const QDBusObjectPath &network);
    QString RequestUserNameAndPassword(const QDBusObjectPath &network, QString &password);
    void Cancel(const QString &reason);

private:
    QHash<QString, QString> m_passphrases;  // network object path -> passphrase
};

// Wi-Fi through iwd on the system D-Bus: device and station state, the scanned networks, and the
// actions scan / power / connect / forget. Every D-Bus call is asynchronous; failures end up in
// lastError and never block or crash.
class WifiModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool powered READ powered NOTIFY poweredChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString connectedName READ connectedName NOTIFY connectedNameChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString connectingName READ connectingName NOTIFY connectingNameChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    enum Role { NameRole = Qt::UserRole + 1, PathRole, BarsRole, SecuredRole, KnownRole, ConnectedRole };

    explicit WifiModel(QObject *parent = nullptr);

    bool available() const { return m_available; }
    bool powered() const { return m_powered; }
    bool scanning() const { return m_scanning; }
    QString connectedName() const { return m_connectedName; }
    QString state() const { return m_state; }
    QString connectingName() const { return m_connectingName; }
    QString lastError() const { return m_lastError; }
    bool active() const { return m_active; }
    // While the quick settings panel is open the list is refreshed every 10 s.
    void setActive(bool active);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void scan();
    Q_INVOKABLE void setPowered(bool on);
    // Both take the network's D-Bus object path (the "path" role): SSIDs are not unique.
    Q_INVOKABLE void connectTo(const QString &path, const QString &passphrase);
    Q_INVOKABLE void forget(const QString &path);
    Q_INVOKABLE void refresh();

signals:
    void availableChanged();
    void poweredChanged();
    void scanningChanged();
    void connectedNameChanged();
    void stateChanged();
    void connectingNameChanged();
    void lastErrorChanged();
    void activeChanged();

private slots:
    void onIwdSignal(const QDBusMessage &message);

private:
    struct NetInfo {
        QString name;
        QString type;
        QString knownPath;
        bool connected = false;
    };
    struct Snapshot {
        QString devicePath;
        QString stationPath;
        bool powered = false;
        bool scanning = false;
        QString state;
        QString connectedNetwork;
        QHash<QString, NetInfo> nets;  // network object path -> properties
    };

    void asyncCall(const QDBusMessage &message, int timeoutMs, const QString &what, std::function<void(bool)> done);
    void registerAgent();
    void onManagedObjects(const QDBusMessage &reply);
    void onOrderedNetworks(const Snapshot &snap, const QDBusMessage &reply);
    void finishRefresh();
    void applySnapshot(const Snapshot &snap, const QList<Network> &networks);
    void setNetworks(const QList<Network> &networks);
    void setLastError(const QString &error);

    WifiAgent *m_agent = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
    QTimer m_pollTimer;
    QTimer m_debounce;
    bool m_active = false;
    bool m_refreshBusy = false;
    bool m_refreshAgain = false;

    QString m_devicePath;
    QString m_stationPath;
    bool m_available = false;
    bool m_powered = false;
    bool m_scanning = false;
    QString m_connectedName;
    QString m_state;
    QString m_connectingName;
    QString m_lastError;

    QList<Network> m_networks;
    QHash<QString, QString> m_knownPaths;  // network object path -> KnownNetwork object path
};
