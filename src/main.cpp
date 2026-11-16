#include "core/MonitorService.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

namespace {
bool writeJson(const QString& path, const QJsonObject& obj) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(obj).toJson()) > 0;
}
bool checkProcessInteractions(QWidget& window) {
    auto* table = window.findChild<QTableView*>("processTable");
    QLineEdit* search = nullptr;
    for (auto* edit : window.findChildren<QLineEdit*>())
        if (edit->accessibleName() == "Search processes")
            search = edit;
    if (!table || !search || !table->model() || table->model()->rowCount() == 0)
        return false;
    const int ownPid = static_cast<int>(QCoreApplication::applicationPid());
    search->setText("SENTINEL_NO_MATCH_9275d7265dd34d74976d");
    const bool excludesUnmatched = table->model()->rowCount() == 0;
    search->setText(QString::number(ownPid));
    bool foundSelf = false;
    for (int row = 0; row < table->model()->rowCount(); ++row)
        if (table->model()->index(row, 1).data(Qt::UserRole).toInt() == ownPid)
            foundSelf = true;
    search->clear();
    table->sortByColumn(3, Qt::DescendingOrder);
    bool sorted = true;
    for (int row = 1; row < table->model()->rowCount(); ++row)
        if (table->model()->index(row - 1, 3).data(Qt::UserRole).toULongLong() <
            table->model()->index(row, 3).data(Qt::UserRole).toULongLong())
            sorted = false;
    table->sortByColumn(2, Qt::DescendingOrder);
    return excludesUnmatched && foundSelf && sorted;
}
} // namespace
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Sentinel");
    QCoreApplication::setOrganizationDomain("sentinel.app");
    QCoreApplication::setApplicationName("Sentinel");
    QCoreApplication::setApplicationVersion("1.0.0");
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/app-icon.svg"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Sentinel — public-API monitoring for Apple Silicon.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(
        {"smoke-test", "Collect live measurements, exercise pages, and exit with a JSON report."});
    parser.addOption(
        {"capture-dir", "Save all ten pages as PNGs after collecting live measurements.", "directory"});
    parser.addOption({"data-dir",
                      "Override the local history/settings directory (useful for isolated checks).",
                      "directory"});
    parser.addOption(
        {"window-size", "Set the inspection window dimensions, for example 1000x700.", "widthxheight"});
    parser.addOption({"observe-seconds", "Observation duration for smoke/capture checks (3–300 seconds).",
                      "seconds", "8.5"});
    parser.addOption({"make-icon",
                      "Render the original SVG application icon to an iconset directory, then exit.",
                      "directory"});
    parser.process(app);
    if (parser.isSet("make-icon")) {
        const QString folder = parser.value("make-icon");
        QDir().mkpath(folder);
        QSvgRenderer renderer(QString(":/app-icon.svg"));
        if (!renderer.isValid())
            return 1;
        for (int size : {16, 32, 128, 256, 512}) {
            for (int scale : {1, 2}) {
                QImage png(size * scale, size * scale, QImage::Format_ARGB32_Premultiplied);
                png.fill(Qt::transparent);
                QPainter painter(&png);
                renderer.render(&painter);
                painter.end();
                const auto name = QString("icon_%1x%1%2.png").arg(size).arg(scale == 2 ? "@2x" : "");
                if (!png.save(folder + "/" + name))
                    return 1;
            }
        }
        return 0;
    }
    const bool check = parser.isSet("smoke-test") || parser.isSet("capture-dir");
    bool durationValid = false;
    const double duration = parser.value("observe-seconds").toDouble(&durationValid);
    if (check && (!durationValid || !std::isfinite(duration) || duration < 3 || duration > 300)) {
        std::fprintf(stderr, "Observation duration must be a finite number between 3 and 300 seconds.\n");
        return 2;
    }
    QTemporaryDir isolated;
    QString dataDir = parser.value("data-dir");
    if (dataDir.isEmpty())
        dataDir =
            check ? isolated.path() : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(dataDir)) {
        std::fprintf(stderr, "Cannot create history directory.\n");
        return 1;
    }
    if (check || parser.isSet("data-dir")) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir);
    }
    sentinel::MonitorService service(dataDir + "/history.sqlite3");
    sentinel::MainWindow window(&service);
    auto* appMenu = window.menuBar()->addMenu("Sentinel");
    auto* about = appMenu->addAction("About Sentinel");
    about->setMenuRole(QAction::AboutRole);
    QObject::connect(about, &QAction::triggered, &window, [&window] {
        QMessageBox::about(&window, "Sentinel",
                           "Sentinel 1.0.0\n\nSystem visibility for Apple Silicon.\n"
                           "Built with C++20 and Qt 6.\n\nMIT License • Sentinel contributors");
    });
    auto* preferences = appMenu->addAction("Settings…");
    preferences->setMenuRole(QAction::PreferencesRole);
    preferences->setShortcut(QKeySequence::Preferences);
    QObject::connect(preferences, &QAction::triggered, &window, [&window] { window.navigateTo(9); });
    appMenu->addSeparator();
    auto* quit = appMenu->addAction("Quit Sentinel");
    quit->setMenuRole(QAction::QuitRole);
    quit->setShortcut(QKeySequence::Quit);
    QObject::connect(quit, &QAction::triggered, &app, &QApplication::quit);
    window.show();
    QSettings settings;
    int count = 0;
    sentinel::Snapshot latest;
    sentinel::SystemInfo system;
    QVector<sentinel::SecurityCheck> security;
    QStringList storageErrors;
    QObject::connect(&service, &sentinel::MonitorService::snapshotReady, &app,
                     [&](const sentinel::Snapshot& s) {
                         latest = s;
                         ++count;
                     });
    QObject::connect(&service, &sentinel::MonitorService::systemInfoReady, &app,
                     [&](const sentinel::SystemInfo& s) { system = s; });
    QObject::connect(&service, &sentinel::MonitorService::securityReady, &app,
                     [&](const QVector<sentinel::SecurityCheck>& s) { security = s; });
    QObject::connect(&service, &sentinel::MonitorService::storageError, &app,
                     [&](const QString& s) { storageErrors.append(s); });
    service.start(check ? 1000 : settings.value("intervalMs", 2000).toInt(),
                  settings.value("retentionDays", 7).toInt());
    if (check) {
        QSize captureSize(1360, 900);
        if (parser.isSet("window-size")) {
            const auto dimensions = parser.value("window-size").split('x');
            if (dimensions.size() == 2)
                captureSize = QSize(dimensions[0].toInt(), dimensions[1].toInt());
        }
        window.resize(captureSize.expandedTo(QSize(1000, 700)));
        auto page = std::make_shared<int>(0);
        auto capture = std::make_shared<std::function<void()>>();
        auto uiChecksPassed = std::make_shared<bool>(false);
        auto captureOk = std::make_shared<bool>(true);
        const QString captureDir = parser.value("capture-dir");
        if (!captureDir.isEmpty())
            *captureOk = QDir().mkpath(captureDir);
        *capture = [&, page, capture, captureDir, uiChecksPassed, captureOk] {
            if (*page < 10) {
                if (*page == 0)
                    *uiChecksPassed = checkProcessInteractions(window);
                window.navigateTo(*page);
                QTimer::singleShot(150, &app, [&, page, capture, captureDir, captureOk] {
                    if (!captureDir.isEmpty()) {
                        const QStringList names{"dashboard", "cpu",     "memory",   "gpu",    "processes",
                                                "network",   "storage", "security", "system", "settings"};
                        *captureOk =
                            window.grab().save(captureDir + "/" + names[*page] + ".png") && *captureOk;
                    }
                    ++*page;
                    QTimer::singleShot(100, &app, *capture);
                });
                return;
            }
            QJsonObject report;
            report["samples"] = count;
            report["cpuPercent"] = latest.cpuPercent ? QJsonValue(*latest.cpuPercent) : QJsonValue();
            report["memoryValid"] = latest.memoryValid;
            report["swapValid"] = latest.swapValid;
            report["uptimeValid"] = latest.uptimeValid;
            report["uptimeSeconds"] = static_cast<double>(latest.uptimeSeconds);
            report["downloadBytesPerSec"] =
                latest.downloadBytesPerSec ? QJsonValue(*latest.downloadBytesPerSec) : QJsonValue();
            report["uploadBytesPerSec"] =
                latest.uploadBytesPerSec ? QJsonValue(*latest.uploadBytesPerSec) : QJsonValue();
            report["memoryTotal"] = static_cast<double>(latest.memoryTotal);
            report["processesVisible"] = latest.processes.size();
            for (const auto& process : latest.processes) {
                if (process.pid == QCoreApplication::applicationPid()) {
                    report["sentinelCpuPercent"] =
                        process.cpuPercent ? QJsonValue(*process.cpuPercent) : QJsonValue();
                    report["sentinelResidentBytes"] = static_cast<double>(process.residentBytes);
                    break;
                }
            }
            report["interfaces"] = latest.interfaces.size();
            report["volumes"] = latest.volumes.size();
            report["gpu"] = system.gpuName;
            report["logicalCores"] = system.logicalCores;
            report["securityChecks"] = security.size();
            report["pagesVisited"] = *page;
            report["processSearchAndNumericSortPassed"] = *uiChecksPassed;
            report["captureSucceeded"] = *captureOk;
            report["storageErrors"] = QJsonArray::fromStringList(storageErrors);
            report["collectionNotes"] = QJsonArray::fromStringList(latest.errors + latest.notices);
            bool passed = count >= 3 && latest.cpuPercent.has_value() && latest.memoryValid &&
                          latest.memoryTotal > 0 && !latest.volumes.isEmpty() && storageErrors.isEmpty() &&
                          *uiChecksPassed && *captureOk;
            report["passed"] = passed;
            if (!captureDir.isEmpty() && !writeJson(captureDir + "/smoke-report.json", report)) {
                passed = false;
                report["passed"] = false;
                report["captureSucceeded"] = false;
            }
            std::fputs(QJsonDocument(report).toJson().constData(), stdout);
            app.exit(passed ? 0 : 1);
            *capture = {}; // break shared function cycle after the callback has run.
        };
        QTimer::singleShot(static_cast<int>(duration * 1000), &app, *capture);
    }
    return app.exec();
}
