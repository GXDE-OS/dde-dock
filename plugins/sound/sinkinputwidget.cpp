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

#include "sinkinputwidget.h"
#include "../widgets/tipswidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QApplication>
#include <DHiDPIHelper>

#define ICON_SIZE   24

DWIDGET_USE_NAMESPACE

const QPixmap getIconFromTheme(const QString &name, const QSize &size, const qreal ratio)
{
    QPixmap ret = QIcon::fromTheme(name, QIcon::fromTheme("application-x-desktop")).pixmap(size * ratio);
    ret.setDevicePixelRatio(ratio);

    return ret;
}

SinkInputWidget::SinkInputWidget(const QString &inputPath, QWidget *parent)
    : QWidget(parent)
    , m_inputInter(new DBusSinkInput(inputPath, this))
    , m_volumeBtnMin(new DImageButton)
    , m_volumeIconMax(new QLabel)
    , m_appBtn(new DImageButton)
    , m_volumeSlider(new VolumeSlider)
{
    const QString iconName = m_inputInter->icon();
    m_appBtn->setAccessibleName("app-" + iconName + "-icon");
    m_appBtn->setPixmap(getIconFromTheme(iconName, QSize(ICON_SIZE, ICON_SIZE), devicePixelRatioF()));

    TipsWidget *titleLabel = new TipsWidget;
    titleLabel->setText(m_inputInter->name());

    m_volumeBtnMin->setAccessibleName("volume-button");
    m_volumeBtnMin->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_volumeBtnMin->setPixmap(DHiDPIHelper::loadNxPixmap("://audio-volume-low-symbolic.svg"));

    m_volumeIconMax->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_volumeIconMax->setPixmap(DHiDPIHelper::loadNxPixmap("://audio-volume-high-symbolic.svg"));

    m_volumeSlider->setAccessibleName("app-" + iconName + "-slider");
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(1000);

    // 应用图标+名称
    QHBoxLayout *appLayout = new QHBoxLayout;
    appLayout->setAlignment(Qt::AlignLeft);
    appLayout->addWidget(m_appBtn);
    appLayout->addSpacing(10);
    appLayout->addWidget(titleLabel);
    appLayout->setSpacing(0);
    appLayout->setMargin(0);

    // 音量图标+slider
    QHBoxLayout *volumeCtrlLayout = new QHBoxLayout;
    volumeCtrlLayout->addSpacing(2);
    volumeCtrlLayout->addWidget(m_volumeBtnMin);
    volumeCtrlLayout->addSpacing(10);
    volumeCtrlLayout->addWidget(m_volumeSlider);
    volumeCtrlLayout->addSpacing(10);
    volumeCtrlLayout->addWidget(m_volumeIconMax);
    volumeCtrlLayout->setSpacing(0);
    volumeCtrlLayout->setMargin(0);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addLayout(appLayout);
    centralLayout->addSpacing(6);
    centralLayout->addLayout(volumeCtrlLayout);
    centralLayout->setSpacing(2);
    centralLayout->setMargin(0);

    connect(m_volumeSlider, &VolumeSlider::valueChanged, this, &SinkInputWidget::setVolume);
    connect(m_volumeSlider, &VolumeSlider::requestPlaySoundEffect, this, &SinkInputWidget::onPlaySoundEffect);
    connect(m_appBtn, &DImageButton::clicked, this, &SinkInputWidget::setMute);
    connect(m_volumeBtnMin, &DImageButton::clicked, this, &SinkInputWidget::setMute);
    connect(m_inputInter, &DBusSinkInput::MuteChanged, this, &SinkInputWidget::setMuteIcon);
    connect(m_inputInter, &DBusSinkInput::VolumeChanged, this, [ = ] {
        m_volumeSlider->setValue(m_inputInter->volume() * 1000);
        changeVolumIcon(m_inputInter->volume());
    });

    setLayout(centralLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(60);

    setMuteIcon();

    emit m_inputInter->VolumeChanged();
}

void SinkInputWidget::setVolume(const int value)
{
    m_inputInter->SetVolumeQueued(double(value) / 1000.0, false);

    changeVolumIcon(double(value) / 1000.0);
}

void SinkInputWidget::setMute()
{
    m_inputInter->SetMuteQueued(!m_inputInter->mute());
}

void SinkInputWidget::setMuteIcon()
{
    if (m_inputInter->mute()) {
        const auto ratio = devicePixelRatioF();
        QPixmap muteIcon = DHiDPIHelper::loadNxPixmap("://audio-volume-muted-symbolic.svg");
        QPixmap appIconSource(getIconFromTheme(m_inputInter->icon(), QSize(ICON_SIZE, ICON_SIZE), devicePixelRatioF()));

        QPixmap temp(appIconSource.size());
        temp.fill(Qt::transparent);
        temp.setDevicePixelRatio(ratio);
        QPainter p1(&temp);
        p1.drawPixmap(0, 0, appIconSource);
        p1.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p1.fillRect(temp.rect(), QColor(0, 0, 0, 40));
        p1.end();
        appIconSource = temp;

        QPainter p(&appIconSource);
        p.drawPixmap(0, 0, muteIcon);

        appIconSource.setDevicePixelRatio(ratio);
        m_appBtn->setPixmap(appIconSource);
        m_volumeBtnMin->setPixmap(DHiDPIHelper::loadNxPixmap("://audio-volume-muted-symbolic.svg"));
    } else {
        m_volumeBtnMin->setPixmap(DHiDPIHelper::loadNxPixmap("://audio-volume-low-symbolic.svg"));
        m_appBtn->setPixmap(getIconFromTheme(m_inputInter->icon(), QSize(ICON_SIZE, ICON_SIZE), devicePixelRatioF()));
    }
}

void SinkInputWidget::onPlaySoundEffect()
{
    // set the mute property to false to play sound effects.
    m_inputInter->SetMuteQueued(false);
}

void SinkInputWidget::changeVolumIcon(const float volume)
{
    QString volumeString;

    if (m_inputInter->mute()) {
        volumeString = "muted";
    } else if (volume >= double(2) / 3) {
        volumeString = "high";
    } else if (volume >= double(1) / 3) {
        volumeString = "medium";
    } else {
        volumeString = "low";
    }

    const QString &iconName = QString("://audio-volume-%1-symbolic.svg").arg(volumeString);
    QPixmap pix = DHiDPIHelper::loadNxPixmap(iconName);

    m_volumeBtnMin->setPixmap(pix);
}
