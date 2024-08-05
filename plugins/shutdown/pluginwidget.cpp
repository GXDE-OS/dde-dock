/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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

#include "pluginwidget.h"

#include <QSvgRenderer>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QIcon>

#include <DStyle>

DWIDGET_USE_NAMESPACE;

PluginWidget::PluginWidget(QWidget *parent)
    : QWidget(parent)
    , m_hover(false)
    , m_pressed(false)
{
    setMouseTracking(true);
    setMinimumSize(PLUGIN_BACKGROUND_MIN_SIZE, PLUGIN_BACKGROUND_MIN_SIZE);
}

QSize PluginWidget::sizeHint() const
{
    return QSize(PLUGIN_BACKGROUND_MAX_SIZE, PLUGIN_BACKGROUND_MAX_SIZE);
}

void PluginWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPixmap pixmap;
    QString iconName = "system-shutdown";
    int iconSize = PLUGIN_ICON_MAX_SIZE;

    QPainter painter(this);

    if (rect().height() > PLUGIN_BACKGROUND_MIN_SIZE) {

        QColor color = QColor::fromRgb(40, 40, 40);;

        if (m_hover) {
            color = QColor::fromRgb(60, 60, 60);
        }

        if (m_pressed) {
            color = QColor::fromRgb(20, 20, 20);
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setOpacity(0.5);

        DStyleHelper dstyle(style());
        const int radius = dstyle.pixelMetric(DStyle::PM_FrameRadius);

        QPainterPath path;

        int minSize = std::min(width(), height());
        QRect rc(0, 0, minSize, minSize);
        rc.moveTo(rect().center() - rc.center());

        path.addRoundedRect(rc, radius, radius);
        painter.fillPath(path, color);
    } else {
        // 最小尺寸时，不画背景，采用深色图标
        iconName.append(PLUGIN_MIN_ICON_NAME);
    }

    painter.setOpacity(1);

    pixmap = loadSvg(iconName, QSize(iconSize, iconSize));
    painter.drawPixmap(rect().center() - pixmap.rect().center() / devicePixelRatioF(), pixmap);
}

const QPixmap PluginWidget::loadSvg(const QString &fileName, const QSize &size) const
{
    const auto ratio = devicePixelRatioF();

    QPixmap pixmap;
    pixmap = QIcon::fromTheme(fileName).pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);

    return pixmap;
}

void PluginWidget::mousePressEvent(QMouseEvent *event)
{
    m_pressed = true;
    update();

    QWidget::mousePressEvent(event);
}

void PluginWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    m_hover = false;
    update();

    QWidget::mouseReleaseEvent(event);
}

void PluginWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = true;
    QWidget::mouseMoveEvent(event);
}

void PluginWidget::leaveEvent(QEvent *event)
{
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        m_hover = false;
        m_pressed = false;
        update();
    }

    QWidget::leaveEvent(event);
}

void PluginWidget::resizeEvent(QResizeEvent *event)
{
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    // 保持横纵比
    if (position == Dock::Bottom || position == Dock::Top) {
        setMaximumWidth(height());
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setMaximumHeight(width());
        setMaximumWidth(QWIDGETSIZE_MAX);
    }

    QWidget::resizeEvent(event);
}
