#include "ui/HistoryChart.h"

#include <QApplication>
#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace sentinel {
namespace {
QString bandwidth(double value) {
    static const char* units[] = {"B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"};
    int unit = 0;
    while (value >= 1024. && unit < 4) {
        value /= 1024.;
        ++unit;
    }
    return QString::number(value, 'f', unit && value < 10. ? 1 : 0) + " " + units[unit];
}
QColor seriesColor(HistoryChart::Mode mode, bool second) {
    switch (mode) {
    case HistoryChart::Memory:
        return QColor("#65d9c0");
    case HistoryChart::Network:
        return QColor(second ? "#9aa9ff" : "#65d9c0");
    case HistoryChart::Disk:
        return QColor("#edc489");
    case HistoryChart::Cpu:
        return QColor("#9aa9ff");
    }
    return QColor("#9aa9ff");
}
} // namespace

HistoryChart::HistoryChart(Mode mode, QWidget* parent) : QWidget(parent), mode_(mode) {
    setMinimumHeight(174);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(QStringList{"CPU activity history", "Memory usage history", "Network throughput history",
                                  "Storage usage history"}
                          .at(mode));
    timer_.setInterval(66);
    timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { updateTimer(); });
}

void HistoryChart::setPoints(const QVector<HistoryPoint>& points, int range) {
    points_ = points;
    range_ = std::max(1, range);
    updateTimer();
    update();
}

void HistoryChart::setMotion(bool on) {
    motion_ = on;
    updateTimer();
    update();
}

void HistoryChart::updateTimer() {
    const bool running = motion_ && range_ <= 3600 && isVisible() && !points_.isEmpty() &&
                         QGuiApplication::applicationState() == Qt::ApplicationActive;
    if (running && !timer_.isActive())
        timer_.start();
    else if (!running)
        timer_.stop();
}

void HistoryChart::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateTimer();
}

void HistoryChart::hideEvent(QHideEvent* event) {
    timer_.stop();
    QWidget::hideEvent(event);
}

void HistoryChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont axisFont = font();
    axisFont.setPointSizeF(9.);
    p.setFont(axisFont);
    const QColor muted("#8b9ab1");
    const QRectF plot(3, 12, std::max(1, width() - (mode_ == Network ? 91 : 57)),
                      std::max(1, height() - 42));
    const qint64 end = QDateTime::currentMSecsSinceEpoch();
    const qint64 start = end - qint64(range_) * 1000;
    auto value = [this](const HistoryPoint& h, bool second) -> std::optional<double> {
        switch (mode_) {
        case Cpu:
            return h.cpuPercent;
        case Memory:
            return h.memoryTotal ? std::optional<double>(100. * h.memoryUsed / h.memoryTotal) : std::nullopt;
        case Network:
            return second ? h.uploadBytesPerSec : h.downloadBytesPerSec;
        case Disk:
            return h.diskTotal ? std::optional<double>(100. * h.diskUsed / h.diskTotal) : std::nullopt;
        }
        return std::nullopt;
    };

    double maximum = mode_ == Network ? 1024. : 100.;
    if (mode_ == Network)
        for (const auto& point : points_)
            if (point.timestampMs >= start && point.timestampMs <= end)
                for (const auto& rate : {point.downloadBytesPerSec, point.uploadBytesPerSec})
                    if (rate && std::isfinite(*rate))
                        maximum = std::max(maximum, *rate * 1.15);

    for (int i = 0; i <= 4; ++i) {
        const double y = plot.top() + plot.height() * i / 4.;
        p.setPen(QPen(QColor(i == 4 ? "#2b374c" : "#222d40"), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(muted);
        const QString label = mode_ == Network ? bandwidth(maximum * (4 - i) / 4.)
                                              : QString::number(maximum * (4 - i) / 4., 'f', 0) + "%";
        p.drawText(QRectF(plot.right() + 10, y - 8, 81, 16), Qt::AlignVCenter, label);
    }

    bool hasMeasurement = false;
    for (int series = 0; series < (mode_ == Network ? 2 : 1); ++series) {
        QVector<QPolygonF> segments;
        bool connected = false;
        qint64 previousTime = 0, previousBucketWidth = 0;
        for (const auto& observation : points_) {
            if (observation.timestampMs < start || observation.timestampMs > end)
                continue;
            const auto measured = value(observation, series == 1);
            if (!measured || !std::isfinite(*measured)) {
                connected = false;
                continue;
            }
            hasMeasurement = true;
            const QPointF point(plot.left() + double(observation.timestampMs - start) / (end - start) *
                                                   plot.width(),
                                plot.bottom() - std::clamp(*measured / maximum, 0., 1.) * plot.height());
            const qint64 gapLimit =
                std::max<qint64>(15000, 2 * std::max(previousBucketWidth, observation.bucketWidthMs));
            if (!connected || observation.timestampMs - previousTime > gapLimit)
                segments.append(QPolygonF{});
            segments.last().append(point);
            connected = true;
            previousTime = observation.timestampMs;
            previousBucketWidth = observation.bucketWidthMs;
        }

        const QColor color = seriesColor(mode_, series == 1);
        // Each recorded segment gets its own fill; missing measurements stay gaps.
        for (const auto& segment : segments) {
            QPainterPath path;
            path.moveTo(segment.first());
            for (int i = 1; i < segment.size(); ++i)
                path.lineTo(segment[i]);
            if (segment.size() > 1 && mode_ != Network) {
                QPainterPath fill = path;
                fill.lineTo(segment.last().x(), plot.bottom());
                fill.lineTo(segment.first().x(), plot.bottom());
                fill.closeSubpath();
                QLinearGradient gradient(0, plot.top(), 0, plot.bottom());
                QColor top = color;
                top.setAlpha(47);
                QColor bottom = color;
                bottom.setAlpha(3);
                gradient.setColorAt(0, top);
                gradient.setColorAt(1, bottom);
                p.fillPath(fill, gradient);
            }
            p.setPen(QPen(color, 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            if (segment.size() == 1) {
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                p.drawEllipse(segment.first(), 2.5, 2.5);
            }
        }
        if (!segments.isEmpty()) {
            const QPointF latest = segments.last().last();
            QColor halo = color;
            halo.setAlpha(32);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(latest, 5., 5.);
            p.setBrush(color);
            p.drawEllipse(latest, 2.5, 2.5);
        }
    }

    p.setPen(muted);
    for (int i = 0; i <= 3; ++i) {
        const qint64 time = start + (end - start) * i / 3;
        const QString label = QDateTime::fromMSecsSinceEpoch(time).toString(range_ > 3600 ? "HH:mm" : "HH:mm:ss");
        const double tickX = i == 0   ? plot.left()
                             : i == 3 ? plot.right() - 70
                                      : plot.left() + plot.width() * i / 3. - 35;
        p.drawText(QRectF(tickX, plot.bottom() + 10, 70, 20),
                   i == 0 ? Qt::AlignLeft : i == 3 ? Qt::AlignRight : Qt::AlignHCenter, label);
    }
    if (!hasMeasurement) {
        p.setPen(QColor("#b4c0d3"));
        p.drawText(plot, Qt::AlignCenter, "Waiting for measurements");
    }
}
} // namespace sentinel
