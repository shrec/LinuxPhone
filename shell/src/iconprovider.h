#pragma once
#include <QFileInfo>
#include <QIcon>
#include <QQuickImageProvider>

// image://icon/<name>: an icon-theme name, or an absolute path as some .desktop files use.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        const int s = requested.width() > 0 ? requested.width() : 192;
        const QIcon icon = QFileInfo(id).isAbsolute() ? QIcon(id) : QIcon::fromTheme(id);
        const QPixmap pm = icon.pixmap(s, s);
        if (size) *size = pm.size();
        return pm;  // null when missing: QML shows a letter tile instead
    }
};
