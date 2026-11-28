#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Battery level and Wi-Fi signal for the status bar. Battery comes from sysfs, Wi-Fi from iwd over
// the system D-Bus; every D-Bus call is asynchronous and any failure just means "not connected".
class StatusModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY batteryChanged)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiName READ wifiName NOTIFY wifiChanged)
    Q_PROPERTY(int wifiBars READ wifiBars NOTIFY wifiChanged)
public:
    explicit StatusModel(QObject *parent = nullptr);
    int batteryPercent() const { return m_percent; }
    bool charging() const { return m_charging; }
    bool wifiConnected() const { return m_wifiConnected; }
    QString wifiName() const { return m_wifiName; }
    int wifiBars() const { return m_wifiBars; }

signals:
    void batteryChanged();
    void wifiChanged();

private:
    void pollBattery();
    void pollWifi();
    void setWifi(bool connected, const QString &name, int bars);
    void onManagedObjects(const QString &stationPath, const QString &networkPath, const QString &name);

    int m_percent = -1;
    bool m_charging = false;
    bool m_wifiConnected = false;
    QString m_wifiName;
    int m_wifiBars = 0;
    bool m_wifiBusy = false;  // a poll is in flight; skip overlapping ones
    QTimer m_batteryTimer;
    QTimer m_wifiTimer;
};
