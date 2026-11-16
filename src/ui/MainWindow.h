#pragma once
#include <QMainWindow>
#include <memory>

namespace sentinel {
class MonitorService;
class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(MonitorService* service, QWidget* parent = nullptr);
    ~MainWindow() override;
    // Used by the integration smoke test to exercise every page.
    void selectPage(int index);
    void navigateTo(int index) { selectPage(index); }

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
} // namespace sentinel
