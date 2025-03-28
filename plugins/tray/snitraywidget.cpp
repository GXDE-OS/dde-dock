/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "snitraywidget.h"
#include "util/themeappicon.h"

#include <QPainter>
#include <QApplication>

#include <xcb/xproto.h>

#define IconSize 16

const QStringList ItemCategoryList {"ApplicationStatus" , "Communications" , "SystemServices", "Hardware"};
const QStringList ItemStatusList {"Passive" , "Active" , "NeedsAttention"};
const QStringList LeftClickInvalidIdList {"sogou-qimpanel",};

SNITrayWidget::SNITrayWidget(const QString &sniServicePath, QWidget *parent)
    : AbstractTrayWidget(parent),
        m_dbusMenuImporter(nullptr),
        m_menu(nullptr),
        m_updateIconTimer(new QTimer(this)),
        m_updateOverlayIconTimer(new QTimer(this)),
        m_updateAttentionIconTimer(new QTimer(this)),
        m_sniServicePath(sniServicePath)
{
    if (m_sniServicePath.startsWith("/") || !m_sniServicePath.contains("/")) {
        qDebug() << "SNI service path invalid";
        return;
    }

    if (m_sniServicePath.contains("/org/ayatana/NotificationItem/")) {
        // fix: duplicate tray for some app
        // some gtk apps use libayatana-appindicator create tray will create xembed and sni duplicate tray
        // not show sni tray which path contains /org/ayatana/NotificationItem/ created by ayatana-appindicator
        qDebug() << "SNI service path created by libayatana-appindicator, duplicate tray with legacy tray";
        return;
    }

    QPair<QString, QString> pair = serviceAndPath(m_sniServicePath);
    m_dbusService = pair.first;
    m_dbusPath = pair.second;

    m_sniInter = new StatusNotifierItem(m_dbusService, m_dbusPath, QDBusConnection::sessionBus(), this);
    m_sniInter->setSync(false);

    if (!m_sniInter->isValid()) {
        qDebug() << "SNI dbus interface is invalid!" << m_dbusService << m_dbusPath << m_sniInter->lastError();
        return;
    }

    m_updateIconTimer->setInterval(100);
    m_updateIconTimer->setSingleShot(true);
    m_updateOverlayIconTimer->setInterval(500);
    m_updateOverlayIconTimer->setSingleShot(true);
    m_updateAttentionIconTimer->setInterval(1000);
    m_updateAttentionIconTimer->setSingleShot(true);

    connect(m_updateIconTimer, &QTimer::timeout, this, &SNITrayWidget::refreshIcon);
    connect(m_updateOverlayIconTimer, &QTimer::timeout, this, &SNITrayWidget::refreshOverlayIcon);
    connect(m_updateAttentionIconTimer, &QTimer::timeout, this, &SNITrayWidget::refreshAttentionIcon);

    // SNI property change
    // thses signals of properties may not be emit automatically!!
    // since the SniInter in on async mode we can not call property's getter function to obtain property directly
    // the way to refresh properties(emit the following signals) is call property's getter function and wait these signals
    connect(m_sniInter, &StatusNotifierItem::AttentionIconNameChanged, this, &SNITrayWidget::onSNIAttentionIconNameChanged);
    connect(m_sniInter, &StatusNotifierItem::AttentionIconPixmapChanged, this, &SNITrayWidget::onSNIAttentionIconPixmapChanged);
    connect(m_sniInter, &StatusNotifierItem::AttentionMovieNameChanged, this, &SNITrayWidget::onSNIAttentionMovieNameChanged);
    connect(m_sniInter, &StatusNotifierItem::CategoryChanged, this, &SNITrayWidget::onSNICategoryChanged);
    connect(m_sniInter, &StatusNotifierItem::IconNameChanged, this, &SNITrayWidget::onSNIIconNameChanged);
    connect(m_sniInter, &StatusNotifierItem::IconPixmapChanged, this, &SNITrayWidget::onSNIIconPixmapChanged);
    connect(m_sniInter, &StatusNotifierItem::IconThemePathChanged, this, &SNITrayWidget::onSNIIconThemePathChanged);
    connect(m_sniInter, &StatusNotifierItem::IdChanged, this, &SNITrayWidget::onSNIIdChanged);
    connect(m_sniInter, &StatusNotifierItem::MenuChanged, this, &SNITrayWidget::onSNIMenuChanged);
    connect(m_sniInter, &StatusNotifierItem::OverlayIconNameChanged, this, &SNITrayWidget::onSNIOverlayIconNameChanged);
    connect(m_sniInter, &StatusNotifierItem::OverlayIconPixmapChanged, this, &SNITrayWidget::onSNIOverlayIconPixmapChanged);
    connect(m_sniInter, &StatusNotifierItem::StatusChanged, this, &SNITrayWidget::onSNIStatusChanged);

    // the following signals can be emit automatically
    // need refresh cached properties in these slots
    connect(m_sniInter, &StatusNotifierItem::NewIcon, [=] {
        m_sniIconName = m_sniInter->iconName();
        m_sniIconPixmap = m_sniInter->iconPixmap();
        m_sniIconThemePath = m_sniInter->iconThemePath();

        m_updateIconTimer->start();
    });
    connect(m_sniInter, &StatusNotifierItem::NewOverlayIcon, [=] {
        m_sniOverlayIconName = m_sniInter->overlayIconName();
        m_sniOverlayIconPixmap = m_sniInter->overlayIconPixmap();
        m_sniIconThemePath = m_sniInter->iconThemePath();

        m_updateOverlayIconTimer->start();
    });
    connect(m_sniInter, &StatusNotifierItem::NewAttentionIcon, [=] {
        m_sniAttentionIconName = m_sniInter->attentionIconName();
        m_sniAttentionIconPixmap = m_sniInter->attentionIconPixmap();
        m_sniIconThemePath = m_sniInter->iconThemePath();

        m_updateAttentionIconTimer->start();
    });
    connect(m_sniInter, &StatusNotifierItem::NewStatus, [=] {
        onSNIStatusChanged(m_sniInter->status());
    });

    initSNIPropertys();
}

SNITrayWidget::~SNITrayWidget()
{
}

QString SNITrayWidget::itemKeyForConfig()
{
    QString key;

    do {
        key = m_sniId;
        if (!key.isEmpty()) {
            break;
        }

        key = QDBusInterface(m_dbusService, m_dbusPath, StatusNotifierItem::staticInterfaceName())
                .property("Id").toString();
        if (!key.isEmpty()) {
            break;
        }

        key = m_sniServicePath;
    } while (false);

    return QString("sni:%1").arg(key);
}

void SNITrayWidget::setActive(const bool active)
{
}

void SNITrayWidget::updateIcon()
{
    m_updateIconTimer->start();
}

void SNITrayWidget::sendClick(uint8_t mouseButton, int x, int y)
{
    switch (mouseButton) {
        case XCB_BUTTON_INDEX_1:
            // left button click invalid
            if (LeftClickInvalidIdList.contains(m_sniId)) {
                showContextMenu(x, y);
            } else {
                m_sniInter->Activate(x, y);
            }
            break;
        case XCB_BUTTON_INDEX_2:
            m_sniInter->SecondaryActivate(x, y);
            break;
        case XCB_BUTTON_INDEX_3:
            showContextMenu(x, y);
            break;
        default:
            qDebug() << "unknown mouse button key";
            break;
    }
}

const QImage SNITrayWidget::trayImage()
{
    return m_pixmap.toImage();
}

bool SNITrayWidget::isValid()
{
    return m_sniInter->isValid();
}

SNITrayWidget::ItemStatus SNITrayWidget::status()
{
    if (!ItemStatusList.contains(m_sniStatus)) {
        m_sniStatus = "Active";
        return ItemStatus::Active;
    }

    return static_cast<ItemStatus>(ItemStatusList.indexOf(m_sniStatus));
}

SNITrayWidget::ItemCategory SNITrayWidget::category()
{
    if (!ItemCategoryList.contains(m_sniCategory)) {
        return UnknownCategory;
    }

    return static_cast<ItemCategory>(ItemCategoryList.indexOf(m_sniCategory));
}

QString SNITrayWidget::toSNIKey(const QString &sniServicePath)
{
    return QString("sni:%1").arg(sniServicePath);
}

bool SNITrayWidget::isSNIKey(const QString &itemKey)
{
    return itemKey.startsWith("sni:");
}

QPair<QString, QString> SNITrayWidget::serviceAndPath(const QString &servicePath)
{
    QStringList list = servicePath.split("/");
    QPair<QString, QString> pair;
    pair.first = list.takeFirst();

    for (auto i : list) {
        pair.second.append("/");
        pair.second.append(i);
    }

    return pair;
}

void SNITrayWidget::initSNIPropertys()
{
    m_sniAttentionIconName = m_sniInter->attentionIconName();
    m_sniAttentionIconPixmap = m_sniInter->attentionIconPixmap();
    m_sniAttentionMovieName = m_sniInter->attentionMovieName();
    m_sniCategory = m_sniInter->category();
    m_sniIconName = m_sniInter->iconName();
    m_sniIconPixmap = m_sniInter->iconPixmap();
    m_sniIconThemePath = m_sniInter->iconThemePath();
    m_sniId = m_sniInter->id();
    m_sniMenuPath = m_sniInter->menu();
    m_sniOverlayIconName = m_sniInter->overlayIconName();
    m_sniOverlayIconPixmap = m_sniInter->overlayIconPixmap();
    m_sniStatus = m_sniInter->status();

    m_updateIconTimer->start();
//    m_updateOverlayIconTimer->start();
//    m_updateAttentionIconTimer->start();
}

void SNITrayWidget::initMenu()
{
    const QString &sniMenuPath = m_sniMenuPath.path();
    if (sniMenuPath.isEmpty()) {
        qDebug() << "Error: current sni menu path is empty of dbus service:" << m_dbusService << "id:" << m_sniId;
        return;
    }

    qDebug() << "using sni service path:" << m_dbusService << "menu path:" << sniMenuPath;

    m_dbusMenuImporter = new DBusMenuImporter(m_dbusService, sniMenuPath, ASYNCHRONOUS, this);

    qDebug() << "generate the sni menu object";

    m_menu = m_dbusMenuImporter->menu();

    qDebug() << "the sni menu obect is:" << m_menu;
}

void SNITrayWidget::refreshIcon()
{
    QPixmap pix = newIconPixmap(Icon);
    if (pix.isNull()) {
        return;
    }

    m_pixmap = pix;
    update();
    Q_EMIT iconChanged();

    if (!isVisible()) {
        Q_EMIT needAttention();
    }
}

void SNITrayWidget::refreshOverlayIcon()
{
    QPixmap pix = newIconPixmap(OverlayIcon);
    if (pix.isNull()) {
        return;
    }

    m_overlayPixmap = pix;
    update();
    Q_EMIT iconChanged();

    if (!isVisible()) {
        Q_EMIT needAttention();
    }
}

void SNITrayWidget::refreshAttentionIcon()
{
    /* TODO: A new approach may be needed to deal with attentionIcon */
    QPixmap pix = newIconPixmap(AttentionIcon);
    if (pix.isNull()) {
        return;
    }

    m_pixmap = pix;
    update();
    Q_EMIT iconChanged();

    if (!isVisible()) {
        Q_EMIT needAttention();
    }
}

void SNITrayWidget::showContextMenu(int x, int y)
{
    // ContextMenu does not work
    if (m_sniMenuPath.path().startsWith("/NO_DBUSMENU")) {
        m_sniInter->ContextMenu(x, y);
    } else {
        if (!m_menu) {
            qDebug() << "context menu has not be ready, init menu";
            initMenu();
        }
        m_menu->popup(QPoint(x, y));
    }
}

void SNITrayWidget::onSNIAttentionIconNameChanged(const QString &value)
{
    m_sniAttentionIconName = value;

    m_updateAttentionIconTimer->start();
}

void SNITrayWidget::onSNIAttentionIconPixmapChanged(DBusImageList value)
{
    m_sniAttentionIconPixmap = value;

    m_updateAttentionIconTimer->start();
}

void SNITrayWidget::onSNIAttentionMovieNameChanged(const QString &value)
{
    m_sniAttentionMovieName = value;

    m_updateAttentionIconTimer->start();
}

void SNITrayWidget::onSNICategoryChanged(const QString &value)
{
    m_sniCategory = value;
}

void SNITrayWidget::onSNIIconNameChanged(const QString &value)
{
    m_sniIconName = value;

    m_updateIconTimer->start();
}

void SNITrayWidget::onSNIIconPixmapChanged(DBusImageList value)
{
    m_sniIconPixmap = value;

    m_updateIconTimer->start();
}

void SNITrayWidget::onSNIIconThemePathChanged(const QString &value)
{
    m_sniIconThemePath = value;

    m_updateIconTimer->start();
}

void SNITrayWidget::onSNIIdChanged(const QString &value)
{
    m_sniId = value;
}

void SNITrayWidget::onSNIMenuChanged(const QDBusObjectPath &value)
{
    m_sniMenuPath = value;
}

void SNITrayWidget::onSNIOverlayIconNameChanged(const QString &value)
{
    m_sniOverlayIconName = value;

    m_updateOverlayIconTimer->start();
}

void SNITrayWidget::onSNIOverlayIconPixmapChanged(DBusImageList value)
{
    m_sniOverlayIconPixmap = value;

    m_updateOverlayIconTimer->start();
}

void SNITrayWidget::onSNIStatusChanged(const QString &status)
{
    if (!ItemStatusList.contains(status) || m_sniStatus == status) {
        return;
    }

    m_sniStatus = status;

    Q_EMIT statusChanged(static_cast<SNITrayWidget::ItemStatus>(ItemStatusList.indexOf(status)));
}

QSize SNITrayWidget::sizeHint() const
{
    return QSize(26, 26);
}

void SNITrayWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    if (m_pixmap.isNull())
        return;

    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);
#ifdef QT_DEBUG
//    painter.fillRect(rect(), Qt::red);
#endif

    const QRectF &rf = QRect(rect());
    const QRectF &rfp = QRect(m_pixmap.rect());
    const QPointF &p = rf.center() - rfp.center() / m_pixmap.devicePixelRatioF();
    painter.drawPixmap(p, m_pixmap);

    if (!m_overlayPixmap.isNull()) {
        painter.drawPixmap(p, m_overlayPixmap);
    }

    painter.end();
}

QPixmap SNITrayWidget::newIconPixmap(IconType iconType)
{
    QPixmap pixmap;
    if (iconType == UnknownIconType) {
        return pixmap;
    }

    QString iconName;
    DBusImageList dbusImageList;

    QString iconThemePath = m_sniIconThemePath;

    switch (iconType) {
        case Icon:
            iconName = m_sniIconName;
            dbusImageList = m_sniIconPixmap;
            break;
        case OverlayIcon:
            iconName = m_sniOverlayIconName;
            dbusImageList = m_sniOverlayIconPixmap;
            break;
        case AttentionIcon:
            iconName = m_sniAttentionIconName;
            dbusImageList = m_sniAttentionIconPixmap;
            break;
        case AttentionMovieIcon:
            iconName = m_sniAttentionMovieName;
            break;
        default:
            break;
    }

    const auto ratio = devicePixelRatioF();
    const int iconSizeScaled = IconSize * ratio;
    do {
        // load icon from sni dbus
        if (!dbusImageList.isEmpty() && !dbusImageList.first().pixels.isEmpty()) {
            for (DBusImage dbusImage : dbusImageList) {
                char *image_data = dbusImage.pixels.data();

                if (QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
                    for (int i = 0; i < dbusImage.pixels.size(); i += 4) {
                        *(qint32*)(image_data + i) = qFromBigEndian(*(qint32*)(image_data + i));
                    }
                }

                QImage image((const uchar*)dbusImage.pixels.constData(), dbusImage.width, dbusImage.height, QImage::Format_ARGB32);
                pixmap = QPixmap::fromImage(image.scaled(iconSizeScaled, iconSizeScaled, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                pixmap.setDevicePixelRatio(ratio);
                if (!pixmap.isNull()) {
                    break;
                }
            }
        }

        // load icon from specified file
        if (!iconThemePath.isEmpty() && !iconName.isEmpty()) {
            QDirIterator it(iconThemePath, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                if (it.fileName().startsWith(iconName, Qt::CaseInsensitive)) {
                    QImage image(it.filePath());
                    pixmap = QPixmap::fromImage(image.scaled(iconSizeScaled, iconSizeScaled, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    pixmap.setDevicePixelRatio(ratio);
                    if (!pixmap.isNull()) {
                        break;
                    }
                }
            }
            if (!pixmap.isNull()) {
                break;
            }
        }

        // load icon from theme
        // Note: this will ensure return a None-Null pixmap
        // so, it should be the last fallback
        if (!iconName.isEmpty()) {
            // ThemeAppIcon::getIcon 会处理高分屏缩放问题
            pixmap = ThemeAppIcon::getIcon(iconName, IconSize, devicePixelRatioF());
            if (!pixmap.isNull()) {
                break;
            }
        }

        if (pixmap.isNull()) {
            qDebug() << "get icon faild!" << iconType;
        }
    } while (false);

    return pixmap;
}
