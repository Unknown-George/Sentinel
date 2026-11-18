#pragma once

#include "core/Types.h"
#include <QTimer>
#include <QWidget>

namespace sentinel {
// Draws only recorded observations. The optional motion advances the time axis,
// never the measured values, and sleeps while the chart or application is hidden.
class HistoryChart final : public QWidget {
  public:
    enum Mode { Cpu, Memory, Network, Disk };
    explicit HistoryChart(Mode mode, QWidget* parent = nullptr);
    void setPoints(const QVector<HistoryPoint>& points, int range);
    void setMotion(bool on);

  protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void updateTimer();
    Mode mode_;
    QVector<HistoryPoint> points_;
    int range_ = 300;
    bool motion_ = true;
    QTimer timer_;
};
} // namespace sentinel
