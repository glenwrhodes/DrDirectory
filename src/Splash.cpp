#include "Splash.h"
#include "Icons.h"
#include "Theme.h"
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

namespace {
const QSize kSize(560, 320);
const char* kAuthor = "Glen Rhodes";
const char* kRepo = "github.com/glenwrhodes/DrDirectory";
} // namespace

SplashScreen::SplashScreen()
{
    const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap pm(kSize * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    setPixmap(pm);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlag(Qt::WindowStaysOnTopHint);
}

void SplashScreen::drawContents(QPainter* p)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::TextAntialiasing);
    const QRectF r(0.5, 0.5, kSize.width() - 1, kSize.height() - 1);

    // panel
    QPainterPath panel;
    panel.addRoundedRect(r, 14, 14);
    QLinearGradient bg(r.topLeft(), r.bottomRight());
    bg.setColorAt(0, QColor(16, 27, 39));
    bg.setColorAt(1, QColor(8, 13, 19));
    p->fillPath(panel, bg);

    // blueprint grid, clipped to the panel
    p->save();
    p->setClipPath(panel);
    p->setPen(QPen(QColor(34, 211, 238, 12), 1));
    for (int x = 0; x < kSize.width(); x += 24)
        p->drawLine(x, 0, x, kSize.height());
    for (int y = 0; y < kSize.height(); y += 24)
        p->drawLine(0, y, kSize.width(), y);
    // cool glow behind the logo
    QRadialGradient glow(QPointF(110, 120), 170);
    glow.setColorAt(0, QColor(34, 211, 238, 40));
    glow.setColorAt(1, QColor(34, 211, 238, 0));
    p->fillRect(QRectF(0, 0, kSize.width(), kSize.height()), glow);
    p->restore();

    p->setPen(QPen(QColor(34, 211, 238, 150), 1));
    p->setBrush(Qt::NoBrush);
    p->drawPath(panel);

    // logo + wordmark
    p->drawPixmap(QPointF(44, 62), Icons::logo(84, p->device()->devicePixelRatioF()));
    QFont title = Theme::displayFont(28, QFont::DemiBold);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 3.5);
    p->setFont(title);
    p->setPen(QColor(234, 246, 255));
    p->drawText(QRectF(150, 66, 380, 44), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("DR DIRECTORY"));
    p->setFont(Theme::displayFont(12));
    p->setPen(Theme::Accent2);
    p->drawText(QRectF(152, 110, 380, 26), Qt::AlignLeft | Qt::AlignVCenter, tr("Disk usage, visualized."));

    // divider
    QLinearGradient line(QPointF(44, 0), QPointF(kSize.width() - 44, 0));
    line.setColorAt(0, QColor(34, 211, 238, 200));
    line.setColorAt(1, QColor(34, 211, 238, 0));
    p->setPen(QPen(QBrush(line), 1));
    p->drawLine(QPointF(44, 176), QPointF(kSize.width() - 44, 176));

    // author + link
    p->setFont(Theme::displayFont(10.5));
    p->setPen(QColor(118, 135, 154));
    p->drawText(QRectF(44, 192, 470, 22), Qt::AlignLeft | Qt::AlignVCenter, tr("Created by"));
    p->setFont(Theme::displayFont(15, QFont::Medium));
    p->setPen(QColor(226, 238, 248));
    p->drawText(QRectF(44, 214, 470, 28), Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(kAuthor));
    p->setFont(Theme::displayFont(10.5));
    p->setPen(Theme::Accent);
    p->drawText(QRectF(44, 246, 470, 22), Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(kRepo));

    // footer
    p->setFont(Theme::displayFont(9));
    p->setPen(QColor(88, 105, 122));
    p->drawText(QRectF(44, kSize.height() - 40, 200, 20), Qt::AlignLeft | Qt::AlignVCenter,
                QStringLiteral("v") + QStringLiteral(DRD_VERSION));
    p->drawText(QRectF(kSize.width() - 344, kSize.height() - 40, 300, 20), Qt::AlignRight | Qt::AlignVCenter,
                QStringLiteral("\u00A9 2026 Tiny Moosh Games Inc.  \u00B7  MIT License"));
}

void SplashScreen::finishAfter(QWidget* mainWindow, int msecs)
{
    QTimer::singleShot(msecs, this, [this, mainWindow] {
        if (mainWindow)
            mainWindow->raise();
        fadeOut();
    });
}

void SplashScreen::mousePressEvent(QMouseEvent*)
{
    fadeOut();
}

void SplashScreen::fadeOut()
{
    if (m_fading)
        return;
    m_fading = true;
    auto anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(350);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, [this] {
        close();
        deleteLater();
    });
    anim->start();
}
