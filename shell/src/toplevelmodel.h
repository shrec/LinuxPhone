#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include "appmodel.h"

class ToplevelHandle;
class ToplevelManager;

// Open app windows, from zwlr_foreign_toplevel_management_v1 (phoc implements it).
// Lets the shell switch to a running app instead of launching a second copy, and send the active one home.
class ToplevelModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    enum Role { AppIdRole = Qt::UserRole + 1, TitleRole, ActivatedRole };
    explicit ToplevelModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Activates (and unminimizes) the first window whose app_id belongs to the desktop entry.
    // Returns false when there is no such window, so the caller should launch the app.
    Q_INVOKABLE bool activate(const QString &desktopId);
    // Minimizes the active window, which reveals the launcher below it.
    Q_INVOKABLE void minimizeActive();
    Q_INVOKABLE void closeActive();

private:
    void toplevelCreated(ToplevelHandle *handle);
    void toplevelDone(ToplevelHandle *handle);
    void toplevelClosed(ToplevelHandle *handle);
    ToplevelHandle *activeHandle() const;

    AppModel m_apps;  // own instance, only used for AppModel::matches
    ToplevelManager *m_manager = nullptr;
    QList<ToplevelHandle *> m_toplevels;  // announced toplevels (first `done` received)
};
