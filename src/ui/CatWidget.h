#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

namespace sentinel {
class CatWidget final : public QWidget {
  public:
    explicit CatWidget(bool small = false);
    void setMotion(bool on);

  protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent*) override;

  private:
    void updateTimer();
    bool small_ = false, motion_ = true, hover_ = false;
    double elapsedSeconds_ = 2.4;
    double eyeOffset_ = 0.;
    QElapsedTimer frameClock_;
    QTimer timer_;
};
} // namespace sentinel
