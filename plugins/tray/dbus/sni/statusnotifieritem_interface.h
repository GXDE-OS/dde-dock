/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp item-inter.xml -a statusnotifieritemadapter -p statusnotifieritemproxy
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef STATUSNOTIFIERITEM_H
#define STATUSNOTIFIERITEM_H

#include "dbusstructures.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.kde.StatusNotifierItem
 */
class StatusNotifierItemInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.kde.StatusNotifierItem"; }

public:
    StatusNotifierItemInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~StatusNotifierItemInterface();

    Q_PROPERTY(QString AttentionIconName READ attentionIconName)
    inline QString attentionIconName() const
    { return qvariant_cast< QString >(property("AttentionIconName")); }

    Q_PROPERTY(DBusImageList AttentionIconPixmap READ attentionIconPixmap)
    inline DBusImageList attentionIconPixmap() const
    { return qvariant_cast< DBusImageList >(property("AttentionIconPixmap")); }

    Q_PROPERTY(QString AttentionMovieName READ attentionMovieName)
    inline QString attentionMovieName() const
    { return qvariant_cast< QString >(property("AttentionMovieName")); }

    Q_PROPERTY(QString Category READ category)
    inline QString category() const
    { return qvariant_cast< QString >(property("Category")); }

    Q_PROPERTY(QString IconName READ iconName)
    inline QString iconName() const
    { return qvariant_cast< QString >(property("IconName")); }

    Q_PROPERTY(DBusImageList IconPixmap READ iconPixmap)
    inline DBusImageList iconPixmap() const
    { return qvariant_cast< DBusImageList >(property("IconPixmap")); }

    Q_PROPERTY(QString Id READ id)
    inline QString id() const
    { return qvariant_cast< QString >(property("Id")); }

    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu)
    inline bool itemIsMenu() const
    { return qvariant_cast< bool >(property("ItemIsMenu")); }

    Q_PROPERTY(QDBusObjectPath Menu READ menu)
    inline QDBusObjectPath menu() const
    { return qvariant_cast< QDBusObjectPath >(property("Menu")); }

    Q_PROPERTY(QString OverlayIconName READ overlayIconName)
    inline QString overlayIconName() const
    { return qvariant_cast< QString >(property("OverlayIconName")); }

    Q_PROPERTY(DBusImageList OverlayIconPixmap READ overlayIconPixmap)
    inline DBusImageList overlayIconPixmap() const
    { return qvariant_cast< DBusImageList >(property("OverlayIconPixmap")); }

    Q_PROPERTY(QString Status READ status)
    inline QString status() const
    { return qvariant_cast< QString >(property("Status")); }

    Q_PROPERTY(QString Title READ title)
    inline QString title() const
    { return qvariant_cast< QString >(property("Title")); }

    Q_PROPERTY(DBusToolTip ToolTip READ toolTip)
    inline DBusToolTip toolTip() const
    { return qvariant_cast< DBusToolTip >(property("ToolTip")); }

    Q_PROPERTY(int WindowId READ windowId)
    inline int windowId() const
    { return qvariant_cast< int >(property("WindowId")); }

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<> Activate(int x, int y)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(x) << QVariant::fromValue(y);
        return asyncCallWithArgumentList(QStringLiteral("Activate"), argumentList);
    }

    inline QDBusPendingReply<> ContextMenu(int x, int y)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(x) << QVariant::fromValue(y);
        return asyncCallWithArgumentList(QStringLiteral("ContextMenu"), argumentList);
    }

    inline QDBusPendingReply<> Scroll(int delta, const QString &orientation)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(delta) << QVariant::fromValue(orientation);
        return asyncCallWithArgumentList(QStringLiteral("Scroll"), argumentList);
    }

    inline QDBusPendingReply<> SecondaryActivate(int x, int y)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(x) << QVariant::fromValue(y);
        return asyncCallWithArgumentList(QStringLiteral("SecondaryActivate"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void NewAttentionIcon();
    void NewIcon();
    void NewOverlayIcon();
    void NewStatus(const QString &status);
    void NewTitle();
    void NewToolTip();
};

namespace com {
  namespace deepin {
        namespace dde {
            typedef ::StatusNotifierItemInterface StatusNotifierItem;
        }
  }
}
#endif