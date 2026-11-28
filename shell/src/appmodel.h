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
    // First of desktopIds that is installed, or an empty string.
    Q_INVOKABLE QString resolve(const QStringList &desktopIds) const;

private:
    const AppEntry *find(const QString &appId) const;
    QList<AppEntry> m_apps;
};
