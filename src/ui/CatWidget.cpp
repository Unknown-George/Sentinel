#include "ui/CatWidget.h"

#include <QApplication>
#include <QEnterEvent>
#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace sentinel {
CatWidget::CatWidget(bool small) : small_(small) {
    setFixedSize(small ? QSize(32, 36) : QSize(146, 112));
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setAccessibleName("Sentinel black cat mascot");
    timer_.setInterval(33);
    timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, [this] {
        const double seconds = std::clamp(frameClock_.restart() / 1000., 0., .1);
        elapsedSeconds_ += seconds;
        const double target = hover_ ? 1. : 0.;
        eyeOffset_ += (target - eyeOffset_) * (1. - std::exp(-12. * seconds));
        if (std::abs(target - eyeOffset_) < .005)
            eyeOffset_ = target;
        update();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { updateTimer(); });
}

void CatWidget::setMotion(bool on) {
    motion_ = on;
    if (!on)
        eyeOffset_ = 0.;
    updateTimer();
    update();
}

void CatWidget::updateTimer() {
    const bool running = motion_ && isVisible() &&
                         QGuiApplication::applicationState() == Qt::ApplicationActive;
    if (running && !timer_.isActive()) {
        frameClock_.start();
        timer_.start();
    } else if (!running)
        timer_.stop();
}

void CatWidget::enterEvent(QEnterEvent* event) {
    hover_ = true;
    QWidget::enterEvent(event);
}

void CatWidget::leaveEvent(QEvent* event) {
    hover_ = false;
    QWidget::leaveEvent(event);
}

void CatWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateTimer();
}

void CatWidget::hideEvent(QHideEvent* event) {
    timer_.stop();
    QWidget::hideEvent(event);
}

void CatWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(width() / 146., height() / 112.);
    if (small_) {
        p.translate(-14, -8);
        p.scale(1.18, 1.18);
    }
    const QColor fur("#080c13");
    QPainterPath tail;
    tail.moveTo(88, 92);
    const double shift = motion_ ? std::sin(elapsedSeconds_ * .56) * 3. : 0.;
    tail.cubicTo(124, 107, 140 + shift, 77, 123 + shift, 64);
    tail.cubicTo(114 + shift, 58, 106 + shift, 65, 112 + shift, 70);
    p.setPen(QPen(QColor("#293449"), 11.5, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawPath(tail);
    p.setPen(QPen(fur, 9.5, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(tail);

    QPainterPath body;
    body.moveTo(49, 96);
    body.cubicTo(32, 98, 30, 72, 42, 57);
    body.lineTo(39, 20);
    body.lineTo(58, 32);
    body.cubicTo(63, 29, 75, 29, 79, 32);
    body.lineTo(98, 19);
    body.lineTo(93, 57);
    body.cubicTo(103, 75, 101, 97, 84, 98);
    body.closeSubpath();
    p.setPen(QPen(QColor("#293449"), 1.2));
    p.setBrush(fur);
    p.drawPath(body);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#222b3c"));
    p.drawPolygon(QPolygonF{QPointF(43, 27), QPointF(54, 35), QPointF(45, 39)});
    p.drawPolygon(QPolygonF{QPointF(93, 27), QPointF(83, 35), QPointF(91, 39)});
    p.setBrush(QColor("#eaf1ff"));
    const double blinkPhase = std::fmod(elapsedSeconds_, 7.3);
    const double eyeHeight = motion_ && blinkPhase < .16 ? 1. : 5.;
    const double dy = -eyeOffset_, dx = 2. * eyeOffset_;
    p.drawRoundedRect(QRectF(50 + dx, 47 + dy + (5. - eyeHeight) / 2., 9, eyeHeight), 2, 2);
    p.drawRoundedRect(QRectF(76 + dx, 47 + dy + (5. - eyeHeight) / 2., 9, eyeHeight), 2, 2);
}
} // namespace sentinel
