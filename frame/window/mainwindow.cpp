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

#include "mainwindow.h"
#include "panel/mainpanelcontrol.h"
#include "controller/dockitemmanager.h"
#include "util/utils.h"

#include <QDebug>
#include <QEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QX11Info>
#include <qpa/qplatformwindow.h>
#include <DStyle>
#include <DPlatformWindowHandle>

#include <X11/X.h>
#include <X11/Xutil.h>

#define SNI_WATCHER_SERVICE "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_PATH "/StatusNotifierWatcher"

#define MAINWINDOW_MAX_SIZE       DOCK_MAX_SIZE
#define MAINWINDOW_MIN_SIZE       (40)
#define DRAG_AREA_SIZE (5)

using org::kde::StatusNotifierWatcher;

class DragWidget : public QWidget
{
    Q_OBJECT

private:
    bool m_dragStatus;
    QPoint m_resizePoint;

public:
    DragWidget(QWidget *parent) : QWidget(parent)
    {
        m_dragStatus = false;
    }

signals:
    void dragPointOffset(QPoint);
    void dragFinished();

private:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_resizePoint = event->globalPos();
            m_dragStatus = true;
            this->grabMouse();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragStatus) {
            QPoint offset = QPoint(QCursor::pos() - m_resizePoint);
            emit dragPointOffset(offset);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (!m_dragStatus)
            return;

        m_dragStatus =  false;
        releaseMouse();
        emit dragFinished();
    }
};

const QPoint rawXPosition(const QPoint &scaledPos)
{
    QScreen const *screen = Utils::screenAtByScaled(scaledPos);

    return screen ? screen->geometry().topLeft() +
           (scaledPos - screen->geometry().topLeft()) *
           screen->devicePixelRatio()
           : scaledPos;
}

const QPoint scaledPos(const QPoint &rawXPos)
{
    QScreen const *screen = Utils::screenAt(rawXPos);

    return screen
           ? screen->geometry().topLeft() +
           (rawXPos - screen->geometry().topLeft()) / screen->devicePixelRatio()
           : rawXPos;
}

MainWindow::MainWindow(QWidget *parent)
    : DBlurEffectWidget(parent),

      m_launched(false),
      m_updatePanelVisible(false),

      m_mainPanel(new MainPanelControl(this)),

      m_platformWindowHandle(this),
      m_wmHelper(DWindowManagerHelper::instance()),

      m_positionUpdateTimer(new QTimer(this)),
      m_expandDelayTimer(new QTimer(this)),
      m_leaveDelayTimer(new QTimer(this)),
      m_shadowMaskOptimizeTimer(new QTimer(this)),

      m_sizeChangeAni(new QVariantAnimation(this)),
      m_posChangeAni(new QVariantAnimation(this)),
      m_panelShowAni(new QPropertyAnimation(this, "pos")),
      m_panelHideAni(new QPropertyAnimation(this, "pos")),
      m_xcbMisc(XcbMisc::instance()),
      m_dbusDaemonInterface(QDBusConnection::sessionBus().interface()),
      m_sniWatcher(new StatusNotifierWatcher(SNI_WATCHER_SERVICE, SNI_WATCHER_PATH, QDBusConnection::sessionBus(), this)),
      m_dragWidget(new DragWidget(this))
{
    setAccessibleName("dock-mainwindow");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setAcceptDrops(true);

    DPlatformWindowHandle::enableDXcbForWindow(this, true);
    m_platformWindowHandle.setEnableBlurWindow(true);
    m_platformWindowHandle.setTranslucentBackground(true);
    m_platformWindowHandle.setWindowRadius(0);
    m_platformWindowHandle.setBorderWidth(0);
    m_platformWindowHandle.setShadowOffset(QPoint(0, 0));
    m_platformWindowHandle.setShadowRadius(0);

    m_settings = &DockSettings::Instance();
    m_xcbMisc->set_window_type(winId(), XcbMisc::Dock);
    m_size = m_settings->m_mainWindowSize;
    m_mainPanel->setDisplayMode(m_settings->displayMode());
    initSNIHost();
    initComponents();
    initConnections();

    resizeMainPanelWindow();

    m_mainPanel->setDelegate(this);
    for (auto item : DockItemManager::instance()->itemList())
        m_mainPanel->insertItem(-1, item);

    m_dragWidget->setMouseTracking(true);
    m_dragWidget->setFocusPolicy(Qt::NoFocus);

    if ((Top == m_settings->position()) || (Bottom == m_settings->position())) {
        m_dragWidget->setCursor(Qt::SizeVerCursor);
    } else {
        m_dragWidget->setCursor(Qt::SizeHorCursor);
    }
}

MainWindow::~MainWindow()
{
    delete m_xcbMisc;
}

void MainWindow::launch()
{
    m_updatePanelVisible = false;
    m_mainPanel->setVisible(false);
    resetPanelEnvironment(false);
    setVisible(false);

    QTimer::singleShot(400, this, [&] {
        m_launched = true;
        m_mainPanel->setVisible(true);
        resetPanelEnvironment(false);
        updateGeometry();
        expand();
    });

    // set strut
    QTimer::singleShot(600, this, [&] {
        setStrutPartial();
    });

    // reset to right environment when animation finished
    QTimer::singleShot(800, this, [&] {
        m_updatePanelVisible = true;
        updatePanelVisible();
    });

    qApp->processEvents();
    QTimer::singleShot(300, this, &MainWindow::show);
}

bool MainWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::Move:
        if (!e->spontaneous())
            QTimer::singleShot(1, this, &MainWindow::positionCheck);
        break;
    default:;
    }

    return QWidget::event(e);
}

void MainWindow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

//    m_platformWindowHandle.setEnableBlurWindow(false);
    m_platformWindowHandle.setShadowOffset(QPoint());
    m_platformWindowHandle.setShadowRadius(0);

    connect(qGuiApp, &QGuiApplication::primaryScreenChanged,
    windowHandle(), [this](QScreen * new_screen) {
        QScreen *old_screen = windowHandle()->screen();
        windowHandle()->setScreen(new_screen);
        // 屏幕变化后可能导致控件缩放比变化，此时应该重设控件位置大小
        // 比如：窗口大小为 100 x 100, 显示在缩放比为 1.0 的屏幕上，此时窗口的真实大小 = 100x100
        // 随后窗口被移动到了缩放比为 2.0 的屏幕上，应该将真实大小改为 200x200。另外，只能使用
        // QPlatformWindow直接设置大小来绕过QWidget和QWindow对新旧geometry的比较。
        const qreal scale = devicePixelRatioF();
        const QPoint screenPos = new_screen->geometry().topLeft();
        const QPoint posInScreen = this->pos() - old_screen->geometry().topLeft();
        const QPoint pos = screenPos + posInScreen * scale;
        const QSize size = this->size() * scale;

        windowHandle()->handle()->setGeometry(QRect(pos, size));
    }, Qt::UniqueConnection);

    windowHandle()->setScreen(qGuiApp->primaryScreen());
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    e->ignore();
    if (e->button() == Qt::RightButton) {
        m_settings->showDockSettingsMenu();
        return;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:        qApp->quit();       break;
#endif
    default:;
    }
}

void MainWindow::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);

    m_leaveDelayTimer->stop();
    if (m_settings->hideState() != Show && m_panelShowAni->state() != QPropertyAnimation::Running)
        m_expandDelayTimer->start();
}

void MainWindow::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    m_expandDelayTimer->stop();
    m_leaveDelayTimer->start();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);

    if (m_settings->hideState() != Show) {
        m_expandDelayTimer->start();
    }
}

void MainWindow::setFixedSize(const QSize &size)
{
    const QPropertyAnimation::State state = m_sizeChangeAni->state();

    if (state == QPropertyAnimation::Stopped && this->size() == size)
        return;

    if (state == QPropertyAnimation::Running)
        return m_sizeChangeAni->setEndValue(size);

    m_sizeChangeAni->setStartValue(this->size());
    m_sizeChangeAni->setEndValue(size);
    m_sizeChangeAni->start();
}

void MainWindow::internalAnimationMove(int x, int y)
{
    const QPropertyAnimation::State state = m_posChangeAni->state();
    const QPoint p = m_posChangeAni->endValue().toPoint();
    const QPoint tp = QPoint(x, y);

    if (state == QPropertyAnimation::Stopped && p == tp)
        return;

    if (state == QPropertyAnimation::Running && p != tp)
        return m_posChangeAni->setEndValue(QPoint(x, y));

    m_posChangeAni->setStartValue(pos());
    m_posChangeAni->setEndValue(tp);
    m_posChangeAni->start();
}

void MainWindow::initSNIHost()
{
    // registor dock as SNI Host on dbus
    QDBusConnection dbusConn = QDBusConnection::sessionBus();
    m_sniHostService = QString("org.kde.StatusNotifierHost-") + QString::number(qApp->applicationPid());
    dbusConn.registerService(m_sniHostService);
    dbusConn.registerObject("/StatusNotifierHost", this);

    if (m_sniWatcher->isValid()) {
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    } else {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon is not exist for now!";
    }
}

void MainWindow::initComponents()
{
    m_positionUpdateTimer->setSingleShot(true);
    m_positionUpdateTimer->setInterval(20);
    m_positionUpdateTimer->start();

    m_expandDelayTimer->setSingleShot(true);
    m_expandDelayTimer->setInterval(m_settings->expandTimeout());

    m_leaveDelayTimer->setSingleShot(true);
    m_leaveDelayTimer->setInterval(m_settings->narrowTimeout());

    m_shadowMaskOptimizeTimer->setSingleShot(true);
    m_shadowMaskOptimizeTimer->setInterval(100);

    m_sizeChangeAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_posChangeAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_panelShowAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_panelHideAni->setEasingCurve(QEasingCurve::InOutCubic);

    QTimer::singleShot(1, this, &MainWindow::compositeChanged);
}

void MainWindow::compositeChanged()
{
    const bool composite = m_wmHelper->hasComposite();

// NOTE(justforlxz): On the sw platform, there is an unstable
// display position error, disable animation solution
#ifndef DISABLE_SHOW_ANIMATION
    const int duration = composite ? 300 : 0;
#else
    const int duration = 0;
#endif

    m_sizeChangeAni->setDuration(duration);
    m_posChangeAni->setDuration(duration);
    m_panelShowAni->setDuration(duration);
    m_panelHideAni->setDuration(duration);

    setComposite(composite);

    m_shadowMaskOptimizeTimer->start();
    m_positionUpdateTimer->start();
}

void MainWindow::internalMove(const QPoint &p)
{
    const bool isHide = m_settings->hideState() == HideState::Hide && !testAttribute(Qt::WA_UnderMouse);
    const bool pos_adjust = m_settings->hideMode() != HideMode::KeepShowing &&
                            isHide &&
                            m_posChangeAni->state() == QVariantAnimation::Stopped;
    if (!pos_adjust)
        return QWidget::move(p);

    QPoint rp = rawXPosition(p);
    const auto ratio = devicePixelRatioF();

    const QRect &r = m_settings->primaryRawRect();
    switch (m_settings->position()) {
    case Left:      rp.setX(r.x());             break;
    case Top:       rp.setY(r.y());             break;
    case Right:     rp.setX(r.right() - 1);     break;
    case Bottom:    rp.setY(r.bottom() - 1);    break;
    }

    int hx = height() * ratio, wx = width() * ratio;
    if (m_settings->hideMode() != HideMode::KeepShowing &&
            isHide &&
            m_panelHideAni->state() == QVariantAnimation::Stopped &&
            m_panelShowAni->state() == QVariantAnimation::Stopped) {
        switch (m_settings->position()) {
        case Top:
        case Bottom:
            hx = 2;
            break;
        case Left:
        case Right:
            wx = 2;
        }
    }

    // using platform window to set real window position
    windowHandle()->handle()->setGeometry(QRect(rp.x(), rp.y(), wx, hx));
}

void MainWindow::initConnections()
{
    connect(m_settings, &DockSettings::dataChanged, m_positionUpdateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::positionChanged, this, &MainWindow::positionChanged);
    connect(m_settings, &DockSettings::autoHideChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowGeometryChanged, this, &MainWindow::updateGeometry, Qt::DirectConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, this, &MainWindow::setStrutPartial, Qt::QueuedConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, [this] { resetPanelEnvironment(true); });
    connect(m_settings, &DockSettings::windowHideModeChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowVisibleChanged, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_settings, &DockSettings::displayModeChanegd, m_positionUpdateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&DockSettings::Instance(), &DockSettings::opacityChanged, this, &MainWindow::setMaskAlpha);
    connect(m_settings, &DockSettings::displayModeChanegd, this, &MainWindow::updateDisplayMode, Qt::QueuedConnection);
//    connect(m_mainPanel, &MainPanelControl::requestRefershWindowVisible, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
//    connect(m_mainPanel, &MainPanelControl::requestWindowAutoHide, m_settings, &DockSettings::setAutoHide);
//    connect(m_mainPanel, &MainPanelControl::geometryChanged, this, &MainWindow::panelGeometryChanged);

    connect(m_positionUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePosition, Qt::QueuedConnection);
    connect(m_expandDelayTimer, &QTimer::timeout, this, &MainWindow::expand, Qt::QueuedConnection);
    connect(m_leaveDelayTimer, &QTimer::timeout, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_shadowMaskOptimizeTimer, &QTimer::timeout, this, &MainWindow::adjustShadowMask, Qt::QueuedConnection);

    connect(m_panelHideAni, &QPropertyAnimation::finished, this, &MainWindow::updateGeometry, Qt::QueuedConnection);
    connect(m_panelHideAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_panelShowAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_posChangeAni, &QVariantAnimation::valueChanged, this, static_cast<void (MainWindow::*)()>(&MainWindow::internalMove));
    connect(m_posChangeAni, &QVariantAnimation::finished, this, static_cast<void (MainWindow::*)()>(&MainWindow::internalMove), Qt::QueuedConnection);

    // to fix qt animation bug, sometimes window size not change
    connect(m_sizeChangeAni, &QVariantAnimation::valueChanged, [ = ](const QVariant & value) {
        QWidget::setFixedSize(value.toSize());
    });

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &MainWindow::compositeChanged, Qt::QueuedConnection);
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::frameMarginsChanged, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

    connect(m_dbusDaemonInterface, &QDBusConnectionInterface::serviceOwnerChanged, this, &MainWindow::onDbusNameOwnerChanged);

    connect(DockItemManager::instance(), &DockItemManager::itemInserted, m_mainPanel, &MainPanelControl::insertItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemRemoved, m_mainPanel, &MainPanelControl::removeItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemUpdated, m_mainPanel, &MainPanelControl::itemUpdated, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestRefershWindowVisible, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestWindowAutoHide, m_settings, &DockSettings::setAutoHide);
    connect(m_mainPanel, &MainPanelControl::itemMoved, DockItemManager::instance(), &DockItemManager::itemMoved, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemAdded, DockItemManager::instance(), &DockItemManager::itemAdded, Qt::DirectConnection);
    connect(m_dragWidget, &DragWidget::dragPointOffset, this, &MainWindow::onMainWindowSizeChanged);
    connect(m_dragWidget, &DragWidget::dragFinished, this, &MainWindow::onDragFinished);
}

const QPoint MainWindow::x11GetWindowPos()
{
    const auto disp = QX11Info::display();

    unsigned int unused;
    int x;
    int y;
    Window unused_window;

    XGetGeometry(disp, winId(), &unused_window, &x, &y, &unused, &unused, &unused, &unused);
    XFlush(disp);

    return QPoint(x, y);
}

void MainWindow::x11MoveWindow(const int x, const int y)
{
    const auto disp = QX11Info::display();

    XMoveWindow(disp, winId(), x, y);
    XFlush(disp);
}

void MainWindow::x11MoveResizeWindow(const int x, const int y, const int w, const int h)
{
    const auto disp = QX11Info::display();

    XMoveResizeWindow(disp, winId(), x, y, w, h);
    XFlush(disp);
}

void MainWindow::positionChanged(const Position prevPos)
{
    // paly hide animation and disable other animation
    m_updatePanelVisible = false;
    clearStrutPartial();
    narrow(prevPos);

    // reset position & layout and slide out
    QTimer::singleShot(200, this, [&] {
        resetPanelEnvironment(false, true);
        updateGeometry();
//        expand();
    });

    // set strut
    QTimer::singleShot(400, this, [&] {
        setStrutPartial();
    });

    // reset to right environment when animation finished
    QTimer::singleShot(600, this, [&] {
        m_updatePanelVisible = true;
        updatePanelVisible();
    });
}

void MainWindow::updatePosition()
{
    // all update operation need pass by timer
    Q_ASSERT(sender() == m_positionUpdateTimer);

    clearStrutPartial();
    updateGeometry();

    // make sure strut partial is set after the size/position animation;
    const int duration = qMax(m_sizeChangeAni->duration(), m_posChangeAni->duration());

    QTimer::singleShot(duration, this, &MainWindow::setStrutPartial);
    QTimer::singleShot(duration, this, &MainWindow::updatePanelVisible);
}

void MainWindow::updateGeometry()
{
    const Position position = m_settings->position();
    QSize size = m_settings->windowSize();

    // DockDisplayMode and DockPosition MUST be set before invoke setFixedSize method of MainPanel
//    m_mainPanel->updateDockDisplayMode(m_settings->displayMode());
    m_mainPanel->setPositonValue(position);
    // this->setFixedSize has been overridden for size animation
    resizeMainPanelWindow();

    bool animation = true;
    bool isHide = m_settings->hideState() == Hide && !testAttribute(Qt::WA_UnderMouse);

    if (isHide) {
        m_sizeChangeAni->stop();
        m_posChangeAni->stop();
        switch (position) {
        case Top:
        case Bottom:    size.setHeight(2);      break;
        case Left:
        case Right:     size.setWidth(2);       break;
        }
        animation = false;
        m_sizeChangeAni->setEndValue(size);
        QWidget::setFixedSize(size);
    } else {
        // this->setFixedSize has been overridden for size animation
        setFixedSize(size);
    }

    const QRect windowRect = m_settings->windowRect(position, isHide);

    if (animation)
        internalAnimationMove(windowRect.x(), windowRect.y());
    else
        internalMove(windowRect.topLeft());

    m_size = m_settings->m_mainWindowSize;

    m_mainPanel->update();
    m_shadowMaskOptimizeTimer->start();

    if ((Top == m_settings->position()) || (Bottom == m_settings->position())) {
        m_dragWidget->setCursor(Qt::SizeVerCursor);
    } else {
        m_dragWidget->setCursor(Qt::SizeHorCursor);
    }
}

void MainWindow::clearStrutPartial()
{
    m_xcbMisc->clear_strut_partial(winId());
}

void MainWindow::setStrutPartial()
{
    // first, clear old strut partial
    clearStrutPartial();

    // reset env
    resetPanelEnvironment(true);

    if (m_settings->hideMode() != Dock::KeepShowing)
        return;

    const auto ratio = devicePixelRatioF();
    const int maxScreenHeight = m_settings->screenRawHeight();
    const int maxScreenWidth = m_settings->screenRawWidth();
    const Position side = m_settings->position();
    const QPoint &p = rawXPosition(m_posChangeAni->endValue().toPoint());
    const QSize &s = m_settings->windowSize();
    const QRect &primaryRawRect = m_settings->primaryRawRect();

    XcbMisc::Orientation orientation = XcbMisc::OrientationTop;
    uint strut = 0;
    uint strutStart = 0;
    uint strutEnd = 0;

    QRect strutArea(0, 0, maxScreenWidth, maxScreenHeight);
    switch (side) {
    case Position::Top:
        orientation = XcbMisc::OrientationTop;
        strut = p.y() + s.height() * ratio;
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setBottom(strut);
        break;
    case Position::Bottom:
        orientation = XcbMisc::OrientationBottom;
        strut = maxScreenHeight - p.y();
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setTop(p.y());
        break;
    case Position::Left:
        orientation = XcbMisc::OrientationLeft;
        strut = p.x() + s.width() * ratio;
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setRight(strut);
        break;
    case Position::Right:
        orientation = XcbMisc::OrientationRight;
        strut = maxScreenWidth - p.x();
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setLeft(p.x());
        break;
    default:
        Q_ASSERT(false);
    }

    qDebug() << "screen info: " << p << strutArea;

    // pass if strut area is intersect with other screen
    int count = 0;
    const QRect pr = m_settings->primaryRect();
    for (auto *screen : qApp->screens()) {
        const QRect sr = screen->geometry();
        if (sr == pr)
            continue;

        if (sr.intersects(strutArea))
            ++count;
    }
    if (count > 0) {
        qWarning() << "strutArea is intersects with another screen.";
        qWarning() << maxScreenHeight << maxScreenWidth << side << p << s;
        return;
    }

    m_xcbMisc->set_strut_partial(winId(), orientation, strut + m_settings->dockMargin(), strutStart, strutEnd);
}

void MainWindow::expand()
{
    qApp->processEvents();

    const auto showAniState = m_panelShowAni->state();
    m_panelHideAni->stop();

    QPoint finishPos = pos();
    QPoint startPos = pos();

    resetPanelEnvironment(true, false);

    if (showAniState != QPropertyAnimation::Running && pos() != m_panelShowAni->currentValue()) {
        const Position position = m_settings->position();
        const QRectF windowRect = m_settings->windowRect(position, true);

        switch (m_settings->position()) {
        case Top:
            startPos.setY(-size().height());
            finishPos.setY(windowRect.top());
            break;
        case Bottom:
            startPos.setY(windowRect.bottom());
            finishPos.setY(windowRect.bottom() - size().height());
            break;
        case Left:
            startPos.setX(-size().width());
            finishPos.setX(windowRect.left());
            break;
        case Right:
            startPos.setX(windowRect.right());
            finishPos.setX(windowRect.right() - size().width());
            break;
        }

        m_panelShowAni->setStartValue(startPos);
        m_panelShowAni->setEndValue(finishPos);
        m_panelShowAni->start();
        m_shadowMaskOptimizeTimer->start();
        m_platformWindowHandle.setShadowRadius(0);
    }
}

void MainWindow::narrow(const Position prevPos)
{
    QPoint finishPos = pos();
    switch (prevPos) {
    case Top:       finishPos.setY(pos().y() - size().height());     break;
    case Bottom:    finishPos.setY(pos().y() + size().height());      break;
    case Left:      finishPos.setX(pos().x() - size().width());      break;
    case Right:     finishPos.setX(pos().x() + size().width());       break;
    }

    m_panelShowAni->stop();
    m_panelHideAni->setStartValue(pos());
    m_panelHideAni->setEndValue(finishPos);
    m_panelHideAni->start();
    m_platformWindowHandle.setShadowRadius(0);
}

void MainWindow::resetPanelEnvironment(const bool visible, const bool resetPosition)
{
    if (!m_launched)
        return;

    // reset environment
    m_sizeChangeAni->stop();
    m_posChangeAni->stop();

    const Position position = m_settings->position();
    const QRect r(m_settings->windowRect(position));

    m_sizeChangeAni->setEndValue(r.size());
    resizeMainPanelWindow();
    QWidget::setFixedSize(r.size());
    m_posChangeAni->setEndValue(r.topLeft());

    if (!resetPosition)
        return;

    QWidget::move(r.topLeft());

    QPoint finishPos(0, 0);
    switch (position) {
    case Top:       finishPos.setY((visible ? 0 : -r.height()));     break;
    case Bottom:    finishPos.setY(visible ? 0 : r.height());      break;
    case Left:      finishPos.setX((visible ? 0 : -r.width()));       break;
    case Right:     finishPos.setX(visible ? 0 : r.width());       break;
    }

    m_mainPanel->move(finishPos);
    m_mainPanel->setPositonValue(position);
}

void MainWindow::updatePanelVisible()
{
    if (!m_updatePanelVisible)
        return;
    if (m_settings->hideMode() == KeepShowing) {
        return expand();
    }

    const Dock::HideState state = m_settings->hideState();

    do {
        if (state != Hide)
            break;

        if (!m_settings->autoHide())
            break;

        QRectF r(pos(), size());
        const int margin = m_settings->dockMargin();
        switch (m_settings->position()) {
        case Dock::Top:
            r.setY(r.y() - margin);
            break;
        case Dock::Bottom:
            r.setHeight(r.height() + margin);
            break;
        case Dock::Left:
            r.setX(r.x() - margin);
            break;
        case Dock::Right:
            r.setWidth(r.width() + margin);
        }
        if (r.contains(QCursor::pos())) {
            break;
        }

        return narrow(m_settings->position());

    } while (false);

    return expand();
}

void MainWindow::adjustShadowMask()
{
    if (!m_launched)
        return;

    if (m_shadowMaskOptimizeTimer->isActive())
        return;

    const bool composite = m_wmHelper->hasComposite();
    const bool isFasion = m_settings->displayMode() == Fashion;

    DStyleHelper dstyle(style());
    const int radius = dstyle.pixelMetric(DStyle::PM_TopLevelWindowRadius);

    m_platformWindowHandle.setWindowRadius(composite && isFasion ? radius : 0);
}

void MainWindow::positionCheck()
{
    if (m_posChangeAni->state() == QPropertyAnimation::Running)
        return;
    if (m_positionUpdateTimer->isActive())
        return;

    const QPoint scaledFrontPos = scaledPos(m_settings->frontendWindowRect().topLeft());

    if (QPoint(pos() - scaledFrontPos).manhattanLength() < 2)
        return;

    // this may cause some position error and animation caton
    //internalMove();
}

void MainWindow::onDbusNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);

    if (name == SNI_WATCHER_SERVICE && !newOwner.isEmpty()) {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon started, register dock to watcher as SNI Host";
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    }
}

void MainWindow::setEffectEnabled(const bool enabled)
{
    if (enabled)
        setMaskColor(LightColor);
    else
        setMaskColor(QColor(55, 63, 71));

    setMaskAlpha(DockSettings::Instance().Opacity());
}

void MainWindow::setComposite(const bool hasComposite)
{
    setEffectEnabled(hasComposite);

    m_sizeChangeAni->setDuration(hasComposite ? 300 : 0);
}

bool MainWindow::appIsOnDock(const QString &appDesktop)
{
    return DockItemManager::instance()->appIsOnDock(appDesktop);
}

void MainWindow::resizeMainWindow()
{
    const Position position = m_settings->position();
    QSize size = m_settings->windowSize();
    const QRect windowRect = m_settings->windowRect(position, false);
    internalMove(windowRect.topLeft());
    resizeMainPanelWindow();
    QWidget::setFixedSize(size);
    m_panelShowAni->setEndValue(pos());
}

void MainWindow::resizeMainPanelWindow()
{
    switch (m_settings->position()) {
    case Dock::Top:
        m_mainPanel->setFixedSize(m_settings->panelSize());
        m_dragWidget->setGeometry(0, this->height() - DRAG_AREA_SIZE, m_settings->panelSize().width(), DRAG_AREA_SIZE);
        break;
    case Dock::Bottom:
        m_mainPanel->setFixedSize(m_settings->panelSize());
        m_dragWidget->setGeometry(0, 0, m_settings->panelSize().width(), DRAG_AREA_SIZE);
        break;
    case Dock::Left:
        m_mainPanel->setFixedSize(m_settings->panelSize());
        m_dragWidget->setGeometry(this->width() - DRAG_AREA_SIZE, 0, DRAG_AREA_SIZE, m_settings->panelSize().height());
        break;
    case Dock::Right:
        m_mainPanel->setFixedSize(m_settings->panelSize());
        m_dragWidget->setGeometry(0, 0, DRAG_AREA_SIZE, m_settings->panelSize().height());
        break;
    default: break;
    }
}

void MainWindow::updateDisplayMode()
{
    m_mainPanel->setDisplayMode(m_settings->displayMode());
}

void MainWindow::onMainWindowSizeChanged(QPoint offset)
{
    if (Dock::Top == m_settings->position()) {
        m_settings->m_mainWindowSize.setHeight(qBound(MAINWINDOW_MIN_SIZE, m_size.height() + offset.y(), MAINWINDOW_MAX_SIZE));
        m_settings->m_mainWindowSize.setWidth(width());
    } else if (Dock::Bottom == m_settings->position()) {
        m_settings->m_mainWindowSize.setHeight(qBound(MAINWINDOW_MIN_SIZE, m_size.height() - offset.y(), MAINWINDOW_MAX_SIZE));
        m_settings->m_mainWindowSize.setWidth(width());
    } else if (Dock::Left == m_settings->position()) {
        m_settings->m_mainWindowSize.setHeight(height());
        m_settings->m_mainWindowSize.setWidth(qBound(MAINWINDOW_MIN_SIZE, m_size.width() + offset.x(), MAINWINDOW_MAX_SIZE));
    } else {
        m_settings->m_mainWindowSize.setHeight(height());
        m_settings->m_mainWindowSize.setWidth(qBound(MAINWINDOW_MIN_SIZE, m_size.width() - offset.x(), MAINWINDOW_MAX_SIZE));
    }

    resizeMainWindow();
}

void MainWindow::onDragFinished()
{
    if (m_size == m_settings->m_mainWindowSize)
        return;

    m_size = m_settings->m_mainWindowSize;
    if (Dock::Top == m_settings->position() || Dock::Bottom == m_settings->position()) {
        m_settings->m_dockInter ->setWindowSize(m_settings->m_mainWindowSize.height());
    } else {
        m_settings->m_dockInter->setWindowSize(m_settings->m_mainWindowSize.width());
    }

    setStrutPartial();
}

#include "mainwindow.moc"
