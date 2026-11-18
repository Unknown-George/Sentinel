#include "ui/MainWindow.h"
#include "core/MonitorService.h"
#include "ui/CatWidget.h"
#include "ui/HistoryChart.h"
#include "ui/ProcessModel.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QEvent>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QShortcut>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QUrl>
#include <QSysInfo>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace sentinel {
namespace {
const QColor blue("#9aa9ff");
QString bytes(quint64 n) {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double v = double(n);
    int i = 0;
    while (v >= 1024 && i < 4) {
        v /= 1024;
        ++i;
    }
    return QString::number(v, 'f', i == 0 ? 0 : (v < 10 ? 2 : 1)) + " " + units[i];
}
QString rate(std::optional<double> n) {
    return n ? bytes(quint64(std::max(0., *n))) + "/s" : QString::fromUtf8("—");
}
QString percent(std::optional<double> n) {
    return n ? QString::number(*n, 'f', 1) + "%" : QString::fromUtf8("—");
}
QString uptime(quint64 t) {
    return QString("%1d %2h %3m").arg(t / 86400).arg(t / 3600 % 24).arg(t / 60 % 60);
}
QLabel* text(const QString& value, const char* object = nullptr) {
    auto* l = new QLabel(value);
    if (object)
        l->setObjectName(object);
    return l;
}
QLabel* note(const QString& value) {
    auto* l = text(value, "note");
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}
QFrame* panel() {
    auto* f = new QFrame;
    f->setObjectName("panel");
    return f;
}
QVBoxLayout* column(QWidget* w, int margin = 20) {
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(margin, margin, margin, margin);
    l->setSpacing(18);
    return l;
}
QWidget* line() {
    auto* f = new QFrame;
    f->setObjectName("separator");
    f->setFixedHeight(1);
    return f;
}
QIcon icon(int type, bool active = false) {
    QPixmap pm(44, 44);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    p.setPen(QPen(active ? blue : QColor("#969bad"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (type) {
    case 0:
        for (auto r : {QRectF(3, 3, 6, 6), QRectF(13, 3, 6, 6), QRectF(3, 13, 6, 6), QRectF(13, 13, 6, 6)})
            p.drawRoundedRect(r, 1, 1);
        break;
    case 1:
        p.drawRoundedRect(QRectF(6, 6, 10, 10), 2, 2);
        p.drawRect(QRectF(9, 9, 4, 4));
        for (int i = 7; i < 17; i += 4) {
            p.drawLine(i, 3, i, 6);
            p.drawLine(i, 16, i, 19);
            p.drawLine(3, i, 6, i);
            p.drawLine(16, i, 19, i);
        }
        break;
    case 2:
        p.drawRoundedRect(QRectF(3, 6, 16, 10), 2, 2);
        for (int x = 6; x < 19; x += 5)
            p.drawRect(QRectF(x, 9, 2, 4));
        for (int x = 5; x < 19; x += 3)
            p.drawLine(x, 16, x, 19);
        break;
    case 3:
        p.drawRoundedRect(QRectF(3, 5, 16, 12), 2, 2);
        p.drawEllipse(QPointF(10, 11), 3, 3);
        p.drawLine(16, 8, 16, 14);
        p.drawLine(6, 17, 6, 20);
        break;
    case 4:
        for (int y = 5; y < 20; y += 6) {
            p.drawEllipse(QPointF(4, y), .6, .6);
            p.drawLine(8, y, 19, y);
        }
        break;
    case 5:
        p.drawEllipse(QPointF(11, 11), 8, 8);
        p.drawEllipse(QRectF(7, 3, 8, 16));
        p.drawLine(3, 11, 19, 11);
        break;
    case 6:
        p.drawRoundedRect(QRectF(3, 4, 16, 14), 3, 3);
        p.drawLine(3, 13, 19, 13);
        p.drawPoint(16, 16);
        break;
    case 7: {
        QPainterPath a;
        a.moveTo(11, 2);
        a.lineTo(19, 6);
        a.lineTo(17, 14);
        a.quadTo(15, 18, 11, 20);
        a.quadTo(7, 18, 5, 14);
        a.lineTo(3, 6);
        a.closeSubpath();
        p.drawPath(a);
        p.drawLine(7, 11, 10, 14);
        p.drawLine(10, 14, 15, 8);
        break;
    }
    case 8:
        p.drawEllipse(QPointF(11, 11), 8, 8);
        p.drawLine(11, 10, 11, 16);
        p.drawPoint(11, 6);
        break;
    default:
        p.drawEllipse(QPointF(11, 11), 4, 4);
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(11, 11);
            p.rotate(i * 45);
            p.drawLine(0, 7, 0, 9);
            p.restore();
        }
        break;
    }
    return QIcon(pm);
}
QTableWidget* table(const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setShowGrid(false);
    t->verticalHeader()->setDefaultSectionSize(42);
    return t;
}
void tableRow(QTableWidget* table, int row, const QStringList& values) {
    for (int col = 0; col < values.size(); ++col) {
        auto* item = table->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table->setItem(row, col, item);
        }
        if (item->text() != values[col]) {
            item->setText(values[col]);
            item->setToolTip(values[col]);
        }
    }
}
} // namespace

struct MainWindow::Impl {
    MainWindow* window;
    MonitorService* service;
    QSettings settings;
    QStackedWidget* stack = nullptr;
    QLabel *title = nullptr, *subtitle = nullptr, *live = nullptr, *errorLabel = nullptr;
    QVBoxLayout* sidebarLayout = nullptr;
    CatWidget* sidebarCat = nullptr;
    QLabel *catCaption = nullptr, *brandCaption = nullptr;
    QString historyError;
    QVector<QPushButton*> nav;
    QHash<QString, QLabel*> labels;
    QHash<QString, QProgressBar*> meters;
    QVector<HistoryChart*> charts;
    QVector<CatWidget*> cats;
    QComboBox* rangeChoice = nullptr;
    QGridLayout* dashboardMetrics = nullptr;
    QVector<QWidget*> dashboardCards;
    int dashboardColumns = 0;
    QPropertyAnimation* transition = nullptr;
    QLineEdit* processSearch = nullptr;
    QPushButton *signatureButton = nullptr, *socketsButton = nullptr;
    QVector<HistoryPoint> history;
    SystemInfo info;
    int range = 300;
    int sampleCount = 0;
    ProcessModel* processModel = nullptr;
    ProcessFilter* proxy = nullptr;
    QTableView* processTable = nullptr;
    QTextEdit* detail = nullptr;
    QLabel* processCount = nullptr;
    QLabel* processNotice = nullptr;
    int selectedPid = 0;
    QString selectedPath;
    QTableWidget *interfaces = nullptr, *volumes = nullptr, *connections = nullptr;
    QLabel* connectionNote = nullptr;
    QPushButton* securityRefresh = nullptr;
    QVector<QProgressBar*> coreBars;
    QGridLayout* coresGrid = nullptr;
    QWidget* coresHost = nullptr;
    QHash<QString, QLabel*> securityStatuses, securityDetails;
    bool motionEnabled = true;
    explicit Impl(MainWindow* w, MonitorService* s) : window(w), service(s) {}
    QLabel* value(const QString& key, const QString& initial = "—", const char* style = "value") {
        auto* l = text(initial, style);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        labels.insert(key, l);
        return l;
    }
    void set(const QString& key, const QString& str) {
        if (labels.contains(key) && labels[key]->text() != str)
            labels[key]->setText(str);
    }
    QFrame* metric(const QString& key, const QString& name, const QString& hint, int glyph = 0) {
        auto* f = panel();
        f->setProperty("metric", true);
        f->setAccessibleName(name);
        auto* l = column(f, 18);
        l->setSpacing(10);
        auto* top = new QHBoxLayout;
        auto* label = text(name, "eyebrow");
        label->setWordWrap(true);
        top->addWidget(label, 1);
        auto* i = text("");
        i->setObjectName("metricIcon");
        i->setFixedSize(30, 30);
        i->setAlignment(Qt::AlignCenter);
        i->setPixmap(icon(glyph, true).pixmap(18, 18));
        top->addWidget(i);
        l->addLayout(top);
        l->addWidget(value(key));
        auto* hintLabel = value(key + "Hint", hint, "note");
        hintLabel->setWordWrap(true);
        hintLabel->setMinimumHeight(16);
        l->addWidget(hintLabel);
        if (key == "dashCpu" || key == "dashMemory" || key == "dashDisk") {
            auto* meter = new QProgressBar;
            meter->setObjectName("meter");
            meter->setProperty("tone", key == "dashMemory" ? "teal" : key == "dashDisk" ? "gold" : "blue");
            meter->setRange(0, 1000);
            meter->setTextVisible(false);
            meter->setFixedHeight(4);
            meters.insert(key, meter);
            l->addWidget(meter);
        } else if (key == "dashDownload") {
            auto* divider = line();
            divider->setFixedHeight(4);
            l->addWidget(divider);
        }
        return f;
    }
    QWidget* page() {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body = new QWidget;
        body->setObjectName("pageBody");
        column(body, 0)->setContentsMargins(0, 4, 10, 8);
        scroll->setWidget(body);
        stack->addWidget(scroll);
        return body;
    }
    QVBoxLayout* layout(QWidget* page) { return qobject_cast<QVBoxLayout*>(page->layout()); }
    QFrame* chartPanel(const QString& heading, HistoryChart::Mode mode, int min = 220) {
        auto* f = panel();
        auto* l = column(f);
        auto* h = new QHBoxLayout;
        h->addWidget(text(heading, "sectionTitle"));
        h->addStretch();
        auto* legend = text(mode == HistoryChart::Network
                                ? "<span style='color:#65d9c0'>↓ Download</span> &nbsp; <span style='color:#9aa9ff'>↑ Upload</span>"
                                : "●  " + QString(mode == HistoryChart::Cpu ? "CPU" : mode == HistoryChart::Memory ? "MEMORY" : "STORAGE"),
                            "chartLegend");
        legend->setStyleSheet(QString("color:%1;").arg(mode == HistoryChart::Memory ? "#65d9c0" : mode == HistoryChart::Disk ? "#edc489" : "#9aa9ff"));
        h->addWidget(legend);
        l->addLayout(h);
        auto* c = new HistoryChart(mode);
        l->setSpacing(12);
        c->setMinimumHeight(min);
        l->addWidget(c);
        charts.append(c);
        return f;
    }
    void rows(QVBoxLayout* l, const QVector<QPair<QString, QString>>& entries) {
        for (const auto& entry : entries) {
            auto* h = new QHBoxLayout;
            h->addWidget(text(entry.first, "note"));
            h->addStretch();
            h->addWidget(value(entry.second, "—", "rowValue"));
            l->addLayout(h);
        }
    }
    void buildDashboard() {
        auto* p = page();
        auto* l = layout(p);
        auto* welcome = panel();
        welcome->setObjectName("welcome");
        auto* wl = new QHBoxLayout(welcome);
        wl->setContentsMargins(22, 14, 22, 14);
        auto* identity = new QVBoxLayout;
        identity->setSpacing(6);
        identity->addWidget(text("ON-DEVICE MONITORING", "eyebrow"));
        identity->addWidget(value("dashModel", "Your Mac, at a glance.", "welcomeTitle"));
        identity->addWidget(value("dashSystem", "Connecting to your system…", "note"));
        wl->addLayout(identity, 1);
        auto* status = text("●  LOCAL & PRIVATE", "privateBadge");
        wl->addWidget(status, 0, Qt::AlignVCenter);
        l->addWidget(welcome);
        dashboardMetrics = new QGridLayout;
        dashboardMetrics->setSpacing(16);
        dashboardCards = {metric("dashCpu", "PROCESSOR", "Measuring activity", 1),
                          metric("dashMemory", "MEMORY", "Physical memory", 2),
                          metric("dashDownload", "NETWORK", "Physical Ethernet / Wi-Fi", 5),
                          metric("dashDisk", "STORAGE", "Startup volume", 6)};
        l->addLayout(dashboardMetrics);
        auto* hero = new QHBoxLayout;
        hero->setSpacing(18);
        hero->addWidget(chartPanel("CPU activity", HistoryChart::Cpu, 176), 3);
        auto* glance = panel();
        auto* gl = column(glance);
        gl->setSpacing(12);
        gl->addWidget(text("At a glance", "sectionTitle"));
        rows(gl, {{"Memory pressure", "dashPressure"},
                  {"Upload", "dashUpload"},
                  {"Uptime", "dashUptime"},
                  {"Processes", "dashProcesses"},
                  {"GPU", "dashGpu"}});
        gl->addStretch();
        gl->addWidget(line());
        gl->addWidget(note("Local measurements. No account, cloud sync, or telemetry."));
        hero->addWidget(glance, 2);
        l->addLayout(hero);
        auto* lower = new QHBoxLayout;
        lower->setSpacing(18);
        lower->addWidget(chartPanel("Memory footprint", HistoryChart::Memory, 114));
        auto* insight = panel();
        auto* il = column(insight);
        il->setSpacing(12);
        auto* it = new QHBoxLayout;
        it->addWidget(text("System observations", "sectionTitle"));
        it->addStretch();
        it->addWidget(text("LIVE", "badge"));
        il->addLayout(it);
        il->addWidget(value("observation", "Collecting the first system sample…", "observation"));
        labels["observation"]->setWordWrap(true);
        il->addWidget(
            value("observationDetail", "Resource status will appear after measurements arrive.", "note"));
        labels["observationDetail"]->setWordWrap(true);
        il->addStretch();
        auto* b = new QPushButton("Review security settings  →");
        b->setObjectName("primary");
        connect(b, &QPushButton::clicked, window, [this] { select(7); });
        il->addWidget(b);
        lower->addWidget(insight);
        l->addLayout(lower);
        l->addStretch();
    }
    void buildCpu() {
        auto* p = page();
        auto* l = layout(p);
        auto* h = new QHBoxLayout;
        h->addWidget(metric("cpuOverall", "OVERALL UTILIZATION", "Difference between CPU tick samples", 1));
        h->addWidget(metric("cpuLogical", "LOGICAL CORES", "Hardware threads", 1));
        h->addWidget(metric("cpuPhysical", "PHYSICAL CORES", "Processor cores", 1));
        l->addLayout(h);
        l->addWidget(chartPanel("Processor activity", HistoryChart::Cpu));
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Per-core utilization", "sectionTitle"));
        fl->addWidget(value("cpuModel", "Reading processor information…", "note"));
        coresHost = new QWidget;
        coresGrid = new QGridLayout(coresHost);
        coresGrid->setContentsMargins(0, 0, 0, 0);
        coresGrid->setSpacing(16);
        fl->addWidget(coresHost);
        l->addWidget(f);
        l->addWidget(note("Overall utilization is busy CPU ticks / total CPU ticks across all logical cores. "
                          "The first sample establishes the baseline. Process CPU uses 100% per fully "
                          "occupied core and can exceed 100%; see Processes to sort by CPU."));
        l->addStretch();
    }
    void buildMemory() {
        auto* p = page();
        auto* l = layout(p);
        auto* h = new QHBoxLayout;
        h->addWidget(metric("memoryUsed", "USED MEMORY", "App + wired + compressed", 2));
        h->addWidget(metric("memoryAvailable", "RECLAIMABLE ESTIMATE", "Physical RAM minus used", 2));
        h->addWidget(metric("memoryPressure", "PRESSURE", "OS memory pressure notification", 2));
        l->addLayout(h);
        l->addWidget(chartPanel("Physical memory usage", HistoryChart::Memory));
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Memory composition", "sectionTitle"));
        rows(fl, {{"Physical memory", "memTotal"},
                  {"Active", "memActive"},
                  {"Wired", "memWired"},
                  {"Compressed (physical footprint)", "memCompressed"},
                  {"Inactive", "memInactive"},
                  {"Free", "memFree"},
                  {"Purgeable (overlaps other categories)", "memPurgeable"},
                  {"Swap used / allocated", "memSwap"}});
        l->addWidget(f);
        l->addWidget(
            note("Used memory reports non-purgeable internal (app) pages + wired + compressor physical "
                 "pages, capped at physical RAM. Available is physical RAM minus this used estimate: "
                 "reclaimable headroom, not free pages or a promise of allocatable memory. Purgeable pages "
                 "overlap other categories and are not added again. Pressure is an OS signal, not inferred "
                 "from a percentage; it may remain Unknown until an event is observed."));
        l->addStretch();
    }
    void buildGpu() {
        auto* p = page();
        auto* l = layout(p);
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Apple Silicon graphics", "sectionTitle"));
        fl->addWidget(value("gpuName", "Reading Metal device…", "heroValue"));
        fl->addWidget(note("Hardware information from the public Metal API."));
        fl->addWidget(line());
        rows(fl,
             {{"Memory architecture", "gpuUnified"}, {"Recommended maximum working set", "gpuWorkingSet"}});
        l->addWidget(f);
        auto* unavailable = panel();
        auto* ul = column(unavailable);
        ul->addWidget(text("Utilization is unavailable", "sectionTitle"));
        ul->addWidget(note(
            "macOS does not expose a stable public API for system-wide Apple Silicon GPU utilization, clock, "
            "temperature, or power. Sentinel does not display estimates as measurements or use private "
            "driver interfaces. Historical GPU utilization is unavailable for the same reason."));
        l->addWidget(unavailable);
        l->addWidget(note("The recommended working-set size is a device-provided guidance value for "
                          "applications using Metal; it is not current GPU memory usage. On unified-memory "
                          "hardware, GPU allocations share physical RAM with the CPU."));
        l->addStretch();
    }
    void updateSelection() {
        auto indices = processTable->selectionModel()->selectedRows();
        if (indices.isEmpty()) {
            selectedPid = 0;
            selectedPath.clear();
            if (signatureButton)
                signatureButton->setEnabled(false);
            if (socketsButton)
                socketsButton->setEnabled(false);
            detail->setPlainText("Select a process to inspect its details. Protected processes may expose "
                                 "only limited information.");
            return;
        }
        auto source = proxy->mapToSource(indices.front());
        if (!source.isValid() || source.row() < 0 || source.row() >= processModel->items.size())
            return;
        const auto& p = processModel->items[source.row()];
        const bool sameProcess = selectedPid == p.pid;
        selectedPid = p.pid;
        selectedPath = p.path;
        signatureButton->setEnabled(!selectedPath.isEmpty());
        socketsButton->setEnabled(selectedPid > 0);
        // Let the user copy inspector text without a live sample clearing selection.
        if (sameProcess && detail->textCursor().hasSelection())
            return;
        const int scroll = detail->verticalScrollBar()->value();
        detail->setPlainText(
            QString("%1\n\nPID  %2    •    User  %3\nCPU  %4    •    Resident memory  %5\n\n%6")
                .arg(p.name)
                .arg(p.pid)
                .arg(p.user, percent(p.cpuPercent), bytes(p.residentBytes),
                     p.path.isEmpty() ? "Executable path unavailable (process exited or access restricted)."
                                      : p.path));
        if (sameProcess)
            detail->verticalScrollBar()->setValue(scroll);
    }
    void updateProcessCount() {
        processCount->setText(QString("%1 of %2 processes").arg(proxy->rowCount()).arg(processModel->rowCount()));
    }
    void buildProcesses() {
        auto* p = page();
        auto* l = layout(p);
        auto* toolbar = new QHBoxLayout;
        auto* search = new QLineEdit;
        processSearch = search;
        search->setPlaceholderText("Search process name, PID, or user…");
        search->setClearButtonEnabled(true);
        search->setAccessibleName("Search processes");
        toolbar->addWidget(search);
        processCount = text("Waiting for process list", "note");
        toolbar->addWidget(processCount);
        l->addLayout(toolbar);
        processModel = new ProcessModel(window);
        proxy = new ProcessFilter(window);
        proxy->setSourceModel(processModel);
        proxy->setSortRole(Qt::UserRole);
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        connect(search, &QLineEdit::textChanged, window,
                [this](const QString& q) {
                    proxy->setFilterFixedString(q);
                    updateProcessCount();
                });
        processTable = new QTableView;
        processTable->setObjectName("processTable");
        processTable->setModel(proxy);
        processTable->setSortingEnabled(true);
        processTable->sortByColumn(2, Qt::DescendingOrder);
        processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        processTable->setSelectionMode(QAbstractItemView::SingleSelection);
        processTable->setShowGrid(false);
        processTable->setAlternatingRowColors(true);
        processTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        processTable->setAccessibleName("Live processes");
        processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        processTable->verticalHeader()->hide();
        processTable->verticalHeader()->setDefaultSectionSize(42);
        processTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int c = 1; c < 5; ++c) {
            processTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
            processTable->setColumnWidth(c, c == 4 ? 130 : 95);
        }
        processTable->setMinimumHeight(330);
        l->addWidget(processTable, 1);
        connect(processTable->selectionModel(), &QItemSelectionModel::selectionChanged, window,
                [this] { updateSelection(); });
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Process inspector", "sectionTitle"));
        detail = new QTextEdit;
        detail->setReadOnly(true);
        detail->setMinimumHeight(120);
        detail->setMaximumHeight(170);
        detail->setPlainText("Select a process to inspect its details. Protected processes may expose only "
                             "limited information.");
        fl->addWidget(detail);
        auto* actions = new QHBoxLayout;
        auto* signature = new QPushButton("Inspect code signature");
        auto* sockets = new QPushButton("View network connections");
        signatureButton = signature;
        socketsButton = sockets;
        signature->setEnabled(false);
        sockets->setEnabled(false);
        actions->addWidget(signature);
        actions->addWidget(sockets);
        actions->addStretch();
        connect(signature, &QPushButton::clicked, window, [this] {
            if (selectedPath.isEmpty()) {
                QMessageBox::information(window, "Executable unavailable",
                                         "Select a process with an accessible executable path.");
                return;
            }
            service->requestSigning(selectedPath);
        });
        connect(sockets, &QPushButton::clicked, window, [this] {
            if (!selectedPid) {
                QMessageBox::information(window, "Select a process",
                                         "Select a process to inspect accessible sockets.");
                return;
            }
            service->requestConnections(selectedPid);
            connectionNote->setText(QString("Inspecting PID %1…").arg(selectedPid));
            select(5);
        });
        fl->addLayout(actions);
        l->addWidget(f);
        processNotice = note("CPU uses 100% per fully occupied core. Memory is resident size. "
                             "Only processes readable by your user are listed.");
        l->addWidget(processNotice);
    }
    void buildNetwork() {
        auto* p = page();
        auto* l = layout(p);
        auto* h = new QHBoxLayout;
        h->addWidget(metric("netDown", "DOWNLOAD", "Active Ethernet / Wi-Fi", 5));
        h->addWidget(metric("netUp", "UPLOAD", "Active Ethernet / Wi-Fi", 5));
        l->addLayout(h);
        l->addWidget(chartPanel("Network throughput", HistoryChart::Network, 170));
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Network interfaces", "sectionTitle"));
        interfaces = table({"Interface", "Local addresses", "Status", "Download", "Upload"});
        interfaces->setMinimumHeight(170);
        fl->addWidget(interfaces);
        fl->addWidget(note("Interface counters include link-layer traffic. The aggregate uses active "
                           "Ethernet/Wi-Fi interfaces; tunnels and bridges are excluded to reduce double "
                           "counting. It is not a billable internet-usage measure."));
        l->addWidget(f);
        auto* c = panel();
        auto* cl = column(c);
        cl->addWidget(text("Process connections", "sectionTitle"));
        connectionNote = note("Select a process in Processes, then choose View network connections. Only "
                              "sockets accessible to your user are inspected.");
        cl->addWidget(connectionNote);
        connections = table({"Protocol", "Local endpoint", "Remote endpoint", "State"});
        connections->setMinimumHeight(170);
        cl->addWidget(connections);
        l->addWidget(c);
        l->addStretch();
    }
    void buildStorage() {
        auto* p = page();
        auto* l = layout(p);
        auto* h = new QHBoxLayout;
        h->addWidget(metric("diskTotal", "CAPACITY", "Startup volume", 6));
        h->addWidget(metric("diskUsed", "USED", "Capacity minus available", 6));
        h->addWidget(metric("diskAvailable", "AVAILABLE", "Space available to this user", 6));
        l->addLayout(h);
        l->addWidget(chartPanel("Startup volume usage", HistoryChart::Disk, 180));
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Mounted volumes", "sectionTitle"));
        volumes = table({"Mount point", "File system", "Capacity", "Available", "Access"});
        volumes->setMinimumHeight(240);
        fl->addWidget(volumes);
        l->addWidget(f);
        l->addWidget(note("APFS volumes can share a container: adding volume capacities or available space "
                          "double-counts shared capacity. Available space is reported by statfs; it may "
                          "differ from Finder's purgeable-space estimate. System-wide disk I/O rates are "
                          "unavailable through the stable public interfaces used by Sentinel."));
        l->addStretch();
    }
    void buildSecurity() {
        auto* p = page();
        auto* l = layout(p);
        auto* h = new QHBoxLayout;
        h->addWidget(note("Visibility into the security configuration of this Mac. Checks are read-only."),
                     1);
        auto* refresh = new QPushButton("Refresh checks");
        securityRefresh = refresh;
        h->addWidget(refresh);
        connect(refresh, &QPushButton::clicked, window, [this] {
            securityRefresh->setEnabled(false);
            securityRefresh->setText("Checking…");
            for (auto* status : securityStatuses)
                status->setText("Checking…");
            service->requestSecurity();
        });
        l->addLayout(h);
        for (const auto& name :
             QStringList{"Firewall", "FileVault", "Gatekeeper", "Sentinel code signature"}) {
            auto* f = panel();
            auto* fl = column(f);
            auto* row = new QHBoxLayout;
            auto* label = text(name, "sectionTitle");
            row->addWidget(label);
            row->addStretch();
            auto* status = text("Checking…", "badge");
            row->addWidget(status);
            securityStatuses.insert(name, status);
            fl->addLayout(row);
            auto* detail = note("Waiting for the operating system.");
            securityDetails.insert(name, detail);
            fl->addWidget(detail);
            l->addWidget(f);
        }
        l->addWidget(note("These settings describe specific protections, not a malware scan or a guarantee "
                          "that the system is secure. Sentinel never enables, disables, or bypasses a macOS "
                          "security control. Unknown results include the reason when available."));
        l->addStretch();
    }
    void buildSystem() {
        auto* p = page();
        auto* l = layout(p);
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Your Mac", "sectionTitle"));
        fl->addWidget(value("sysModel", "Reading hardware information…", "heroValue"));
        fl->addWidget(value("sysOs", "—", "note"));
        fl->addWidget(line());
        rows(fl, {{"Processor", "sysCpu"},
                  {"Architecture", "sysArch"},
                  {"Physical / logical cores", "sysCores"},
                  {"Physical memory", "sysMemory"},
                  {"Graphics", "sysGpu"},
                  {"Host name", "sysHost"},
                  {"System uptime", "sysUptime"}});
        l->addWidget(f);
        auto* identity = panel();
        auto* il = column(identity);
        il->addWidget(text("A clear view of your system", "sectionTitle"));
        il->addWidget(note("Sentinel combines public Mach, libproc, IOKit, Metal, and filesystem interfaces "
                           "with on-device historical storage. Hardware data and monitoring history remain "
                           "on this Mac. No privileged helper is installed."));
        l->addWidget(identity);
        l->addStretch();
    }
    void buildSettings() {
        auto* p = page();
        auto* l = layout(p);
        auto* f = panel();
        auto* fl = column(f);
        fl->addWidget(text("Monitoring preferences", "sectionTitle"));
        auto* refresh = new QComboBox;
        refresh->addItem("Every 1 second", 1000);
        refresh->addItem("Every 2 seconds", 2000);
        refresh->addItem("Every 5 seconds", 5000);
        refresh->setCurrentIndex(std::max(0, refresh->findData(settings.value("intervalMs", 2000).toInt())));
        auto* r = new QHBoxLayout;
        r->addWidget(text("Refresh interval"));
        r->addStretch();
        r->addWidget(refresh);
        fl->addLayout(r);
        connect(refresh, &QComboBox::currentIndexChanged, window, [this, refresh] {
            int ms = refresh->currentData().toInt();
            settings.setValue("intervalMs", ms);
            service->setInterval(ms);
        });
        auto* retention = new QComboBox;
        retention->addItem("1 day", 1);
        retention->addItem("7 days", 7);
        retention->addItem("30 days", 30);
        retention->setCurrentIndex(
            std::max(0, retention->findData(settings.value("retentionDays", 7).toInt())));
        auto* rh = new QHBoxLayout;
        rh->addWidget(text("Keep monitoring history"));
        rh->addStretch();
        rh->addWidget(retention);
        fl->addLayout(rh);
        connect(retention, &QComboBox::currentIndexChanged, window, [this, retention] {
            int days = retention->currentData().toInt();
            settings.setValue("retentionDays", days);
            service->setRetention(days);
        });
        fl->addWidget(line());
        auto* motion = new QCheckBox("Smooth charts, page transitions, and mascot animation");
        motionEnabled = settings.value("animateMascot", true).toBool();
        motion->setChecked(motionEnabled);
        fl->addWidget(motion);
        fl->addWidget(note("Animation also respects Reduce Motion in macOS Accessibility settings."));

        connect(motion, &QCheckBox::toggled, window, [this](bool b) {
            motionEnabled = b;
            settings.setValue("animateMascot", b);
            applyMotion();
        });
        l->addWidget(f);
        auto* historyPanel = panel();
        auto* hl = column(historyPanel);
        hl->addWidget(text("On-device data", "sectionTitle"));
        hl->addWidget(note("History stores only timestamps and aggregate CPU, memory, network, and "
                           "startup-volume metrics. Process names, executable paths, socket endpoints, and "
                           "security results are not written to the history database."));
        auto* location = note(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        hl->addWidget(location);
        auto* buttons = new QHBoxLayout;
        auto* folder = new QPushButton("Open data folder");
        auto* clear = new QPushButton("Clear monitoring history…");
        clear->setObjectName("destructive");
        buttons->addWidget(folder);
        buttons->addWidget(clear);
        buttons->addStretch();
        connect(folder, &QPushButton::clicked, window, [] {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
        });
        connect(clear, &QPushButton::clicked, window, [this] {
            if (QMessageBox::question(window, "Clear monitoring history?",
                                      "Delete all recorded monitoring samples from this Mac? New "
                                      "measurements will continue to be recorded.",
                                      QMessageBox::Cancel | QMessageBox::Yes,
                                      QMessageBox::Cancel) == QMessageBox::Yes)
                service->clearHistory();
        });
        hl->addLayout(buttons);
        l->addWidget(historyPanel);
        l->addWidget(note(QString("Sentinel %1 • C++20 / Qt 6 • Designed for Apple Silicon and macOS 14 or later.")
                              .arg(QCoreApplication::applicationVersion())));
        l->addStretch();
    }
    void adjustSidebar(int height) {
        if (!sidebarLayout)
            return;
        const bool compact = height < 820;
        sidebarLayout->setSpacing(compact ? 4 : 7);
        for (auto* button : nav)
            button->setFixedHeight(compact ? 34 : 40);
        brandCaption->setVisible(height >= 740);
        catCaption->setVisible(height >= 820);
        sidebarCat->setVisible(height >= 800);
        sidebarCat->setFixedSize(height < 820 ? QSize(91, 70) : QSize(146, 112));
        if (dashboardMetrics) {
            const int count = stack->width() < 900 ? 2 : 4;
            if (dashboardColumns != count) {
                dashboardColumns = count;
                for (auto* card : dashboardCards)
                    dashboardMetrics->removeWidget(card);
                for (int i = 0; i < dashboardCards.size(); ++i)
                    dashboardMetrics->addWidget(dashboardCards[i], i / count, i % count);
                for (int i = 0; i < 4; ++i)
                    dashboardMetrics->setColumnStretch(i, i < count ? 1 : 0);
            }
        }
    }
    void applyMotion() {
        const bool enabled = motionEnabled && !info.reducedMotion;
        for (auto* cat : cats)
            cat->setMotion(enabled);
        for (auto* chart : charts)
            chart->setMotion(enabled);
        if (!enabled && transition) {
            transition->stop();
            transition->deleteLater();
            transition = nullptr;
            stack->setGraphicsEffect(nullptr);
        }
    }
    void animatePage() {
        if (transition) {
            transition->stop();
            transition->deleteLater();
            transition = nullptr;
            stack->setGraphicsEffect(nullptr);
        }
        if (!motionEnabled || info.reducedMotion || !window->isVisible())
            return;
        auto* opacity = new QGraphicsOpacityEffect(stack);
        stack->setGraphicsEffect(opacity);
        transition = new QPropertyAnimation(opacity, "opacity", stack);
        transition->setDuration(120);
        transition->setStartValue(0.72);
        transition->setEndValue(1.0);
        transition->setEasingCurve(QEasingCurve::OutCubic);
        connect(transition, &QPropertyAnimation::finished, window, [this] {
            transition->deleteLater();
            transition = nullptr;
            stack->setGraphicsEffect(nullptr);
        });
        transition->start();
    }
    void select(int i) {
        if (i < 0 || i >= stack->count())
            return;
        const bool changed = stack->currentIndex() != i;
        stack->setCurrentIndex(i);
        if (changed)
            animatePage();
        for (int j = 0; j < nav.size(); ++j) {
            nav[j]->setChecked(j == i);
            nav[j]->setIcon(icon(j, j == i));
        }
        const QStringList names = {"Overview",           "CPU",     "Memory",  "GPU",
                                   "Processes",          "Network", "Storage", "Security Center",
                                   "System Information", "Settings"};
        const QStringList sub = {"A quiet, clear view of your Mac.",
                                 "Every core. Every workload.",
                                 "Understand your physical memory.",
                                 "Graphics hardware, clearly reported.",
                                 "Know what is running.",
                                 "Traffic and connections at a glance.",
                                 "Space for what comes next.",
                                 "Know which protections are enabled.",
                                 "The hardware behind your work.",
                                 "Make Sentinel feel at home."};
        title->setText(names[i]);
        subtitle->setText(sub[i]);
        rangeChoice->setVisible(i <= 2 || i == 5 || i == 6);
        updateCharts();
    }
    void updateCharts() {
        for (auto* c : charts)
            if (c->isVisible())
                c->setPoints(history, range);
    }
    void snapshotReceived(const Snapshot& s) {
        ++sampleCount;
        live->setText("●  LIVE  ·  " + s.timestamp.toLocalTime().toString("HH:mm:ss"));
        live->setToolTip("Last sample received " + s.timestamp.toLocalTime().toString("HH:mm:ss"));
        meters["dashCpu"]->setValue(s.cpuPercent ? int(*s.cpuPercent * 10) : 0);
        meters["dashMemory"]->setValue(s.memoryValid && s.memoryTotal ? int(1000. * s.memoryUsed / s.memoryTotal) : 0);
        QStringList errors = s.errors;
        if (!historyError.isEmpty())
            errors.append("History storage: " + historyError);
        errorLabel->setVisible(!errors.isEmpty());
        errorLabel->setText(errors.join("  •  "));
        set("dashCpu", percent(s.cpuPercent));
        set("dashCpuHint",
            s.cpuPercent ? QString("%1 logical cores").arg(info.logicalCores) : "Establishing CPU baseline");
        set("cpuOverall", percent(s.cpuPercent));
        set("dashMemory", s.memoryValid ? bytes(s.memoryUsed) : "—");
        set("dashMemoryHint", s.memoryValid ? QString("of %1 physical RAM").arg(bytes(s.memoryTotal))
                                            : "Memory sample unavailable");
        set("dashDownload", rate(s.downloadBytesPerSec));
        set("dashDownloadHint", s.uploadBytesPerSec ? "↑ " + rate(s.uploadBytesPerSec)
                                                    : (sampleCount == 1 ? "Establishing network baseline"
                                                                        : "No physical uplink available"));
        set("dashUpload", rate(s.uploadBytesPerSec));
        set("netDown", rate(s.downloadBytesPerSec));
        set("netUp", rate(s.uploadBytesPerSec));
        set("dashPressure", s.memoryPressure);
        set("dashUptime", s.uptimeValid ? uptime(s.uptimeSeconds) : "Unavailable");
        set("sysUptime", s.uptimeValid ? uptime(s.uptimeSeconds) : "Unavailable");
        set("dashProcesses", QString::number(s.processes.size()));
        set("memoryUsed", s.memoryValid ? bytes(s.memoryUsed) : "—");
        set("memoryAvailable", s.memoryValid ? bytes(s.memoryAvailable) : "—");
        set("memoryPressure", s.memoryPressure);
        set("memoryPressureHint",
            s.memoryPressure == "Unknown" ? "No pressure signal observed" : "Reported by macOS");
        const QVector<QPair<QString, quint64>> mem = {
            {"memTotal", s.memoryTotal},        {"memActive", s.memoryActive},
            {"memWired", s.memoryWired},        {"memCompressed", s.memoryCompressed},
            {"memInactive", s.memoryInactive},  {"memFree", s.memoryFree},
            {"memPurgeable", s.memoryPurgeable}};
        for (auto m : mem)
            set(m.first, s.memoryValid ? bytes(m.second) : "—");
        set("memSwap", s.swapValid ? bytes(s.swapUsed) + " / " + bytes(s.swapTotal) : "Unavailable");
        while (coreBars.size() < s.corePercent.size()) {
            int idx = coreBars.size();
            auto* item = new QWidget;
            auto* il = column(item, 0);
            il->setSpacing(7);
            il->addWidget(text(QString("CORE %1").arg(idx + 1), "eyebrow"));
            auto* bar = new QProgressBar;
            bar->setRange(0, 1000);
            bar->setTextVisible(true);
            bar->setFixedHeight(17);
            coreBars.append(bar);
            il->addWidget(bar);
            coresGrid->addWidget(item, idx / 4, idx % 4);
        }
        for (int i = 0; i < coreBars.size(); ++i) {
            const bool valid = i < s.corePercent.size();
            coreBars[i]->setValue(valid ? int(s.corePercent[i] * 10) : 0);
            coreBars[i]->setFormat(valid ? QString::number(s.corePercent[i], 'f', 1) + "%" : "—");
        }
        processModel->replace(s.processes);
        if (selectedPid)
            updateSelection();
        updateProcessCount();
        processCount->setToolTip(s.notices.join('\n'));
        processNotice->setText("CPU uses 100% per fully occupied core. Memory is resident size. " +
                               s.notices.join(" "));
        interfaces->setRowCount(s.interfaces.size());
        for (int i = 0; i < s.interfaces.size(); ++i) {
            const auto& v = s.interfaces[i];
            tableRow(interfaces, i,
                     {v.name, v.address, v.up ? (v.loopback ? "Loopback" : "Active") : "Down",
                      rate(v.downloadBytesPerSec), rate(v.uploadBytesPerSec)});
        }
        volumes->setRowCount(s.volumes.size());
        const Volume* startup = nullptr;
        for (int i = 0; i < s.volumes.size(); ++i) {
            const auto& v = s.volumes[i];
            tableRow(volumes, i,
                     {v.mountPoint, v.filesystem, bytes(v.totalBytes), bytes(v.availableBytes),
                      v.readOnly ? "Read only" : "Read / write"});
            if (v.mountPoint == "/System/Volumes/Data")
                startup = &v;
            else if (!startup && v.mountPoint == "/")
                startup = &v;
        }
        meters["dashDisk"]->setValue(startup && startup->totalBytes ?
            int(1000. * (startup->totalBytes - std::min(startup->totalBytes, startup->availableBytes)) / startup->totalBytes) : 0);
        if (!startup) {
            for (const auto& key : {"dashDisk", "diskTotal", "diskUsed", "diskAvailable"})
                set(key, "Unavailable");
            set("dashDiskHint", "Startup volume unavailable");
        }
        if (startup) {
            auto used = startup->totalBytes - std::min(startup->totalBytes, startup->availableBytes);
            set("dashDisk",
                startup->totalBytes ? QString::number(100. * used / startup->totalBytes, 'f', 0) + "%" : "—");
            set("dashDiskHint", bytes(startup->availableBytes) + " available");
            set("diskTotal", bytes(startup->totalBytes));
            set("diskUsed", bytes(used));
            set("diskAvailable", bytes(startup->availableBytes));
        }
        if (s.memoryPressure == "Critical" || s.memoryPressure == "Warning") {
            set("observation", "Memory pressure needs attention");
            set("observationDetail",
                "macOS reported elevated memory pressure. Inspect Processes to find memory-heavy workloads.");
        } else if (s.cpuPercent && *s.cpuPercent > 85) {
            set("observation", "Your CPU is working hard");
            set("observationDetail", "Overall processor utilization is above 85%. Open CPU or Processes to "
                                     "inspect the current workload.");
        } else if (!s.errors.isEmpty()) {
            set("observation", "Some measurements are unavailable");
            set("observationDetail", "macOS limited access to one or more system measurements. Available "
                                     "values continue updating.");
        } else {
            set("observation", "Monitoring is up to date");
            set("observationDetail",
                s.memoryPressure == "Unknown"
                    ? "CPU, memory, network, and storage samples are current. Memory pressure remains "
                      "unknown until macOS provides a signal."
                    : "System measurements are current. Open Security Center to review protection settings.");
        }
        HistoryPoint hp;
        hp.timestampMs = s.timestamp.toMSecsSinceEpoch();
        hp.cpuPercent = s.cpuPercent;
        hp.memoryUsed = s.memoryUsed;
        hp.memoryTotal = s.memoryValid ? s.memoryTotal : 0;
        hp.downloadBytesPerSec = s.downloadBytesPerSec;
        hp.uploadBytesPerSec = s.uploadBytesPerSec;
        if (startup) {
            hp.diskTotal = startup->totalBytes;
            hp.diskUsed = startup->totalBytes - std::min(startup->totalBytes, startup->availableBytes);
        }
        history.append(hp);
        qint64 min = hp.timestampMs - qint64(range) * 1000;
        while (!history.isEmpty() && (history.front().timestampMs < min || history.size() > 6000))
            history.removeFirst();
        updateCharts();
    }
    void infoReceived(const SystemInfo& s) {
        info = s;
        set("cpuLogical", QString::number(s.logicalCores));
        set("cpuPhysical", QString::number(s.physicalCores));
        set("cpuModel", s.cpuModel);
        set("gpuName", s.gpuName.isEmpty() ? "No Metal device available" : s.gpuName);
        set("dashGpu", s.gpuName.isEmpty() ? "Unavailable" : s.gpuName);
        set("gpuUnified", s.gpuName.isEmpty() ? "Unavailable"
                                              : (s.gpuUnifiedMemory ? "Unified memory" : "Discrete memory"));
        set("gpuWorkingSet", s.gpuRecommendedWorkingSet ? bytes(s.gpuRecommendedWorkingSet) : "Unavailable");
        set("sysModel", s.model);
        set("sysOs", s.osVersion);
        set("sysCpu", s.cpuModel);
        set("sysArch", s.architecture);
        set("sysCores", QString("%1 / %2").arg(s.physicalCores).arg(s.logicalCores));
        set("sysMemory", bytes(s.physicalMemory));
        set("sysGpu", s.gpuName.isEmpty() ? "Unavailable" : s.gpuName);
        set("sysHost", s.hostname);
        set("dashModel", s.cpuModel.isEmpty() ? s.model : s.cpuModel);
        set("dashSystem", QString("macOS %1  ·  %2 memory  ·  %3 cores").arg(QSysInfo::productVersion(), bytes(s.physicalMemory)).arg(s.logicalCores));
        applyMotion();
    }
    void securityReceived(const QVector<SecurityCheck>& checks) {
        securityRefresh->setEnabled(true);
        securityRefresh->setText("Refresh checks");
        for (const auto& c : checks) {
            QString name = c.name;
            if (!securityStatuses.contains(name)) {
                if (name.contains("signature", Qt::CaseInsensitive) ||
                    name.contains("signing", Qt::CaseInsensitive))
                    name = "Sentinel code signature";
                else if (name.contains("firewall", Qt::CaseInsensitive))
                    name = "Firewall";
                else if (name.contains("filevault", Qt::CaseInsensitive))
                    name = "FileVault";
                else if (name.contains("gatekeeper", Qt::CaseInsensitive))
                    name = "Gatekeeper";
            }
            if (!securityStatuses.contains(name))
                continue;
            auto* status = securityStatuses[name];
            status->setText(c.status);
            status->setStyleSheet(QString("color:%1;background:%2;border-radius:6px;padding:5px 9px;")
                                      .arg(c.level == "enabled"    ? "#81d3bb"
                                           : c.level == "disabled" ? "#e9b77b"
                                                                   : "#b2b8c9",
                                           c.level == "enabled" ? "#1d3430" : "#2d2f3b"));
            securityDetails[name]->setText(c.detail);
        }
    }
    void build() {
        window->setWindowTitle("Sentinel");
        window->resize(1360, 940);
        window->setMinimumSize(1000, 700);
        window->setUnifiedTitleAndToolBarOnMac(false);
        window->setStyleSheet(R"(
        QMainWindow,QWidget#root { background:#10151f; color:#e9eef9; }
        QWidget { color:#dce5f4; font-family:".AppleSystemUIFont","SF Pro Text","Helvetica Neue"; font-size:13px; }
        QWidget#sidebar { background:#0e131d; border-right:1px solid #253044; }
        QWidget#pageBody,QScrollArea,QScrollArea>QWidget>QWidget { background:#10151f; }
        QFrame#panel { background:#171e2b; border:1px solid #2a3549; border-radius:14px; }
        QFrame#welcome { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #202b43,stop:1 #182332); border:1px solid #34415d; border-radius:14px; }
        QFrame[metric="true"] { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #1c2535,stop:1 #171e2b); }
        QFrame#separator { background:#253044; border:0; }
        QLabel { background:transparent; border:0; }
        QLabel#brand { font-size:23px; font-weight:650; letter-spacing:0.4px; color:#f2f5ff; }
        QLabel#title { font-size:32px; font-weight:650; letter-spacing:-0.8px; color:#f0f4ff; }
        QLabel#subtitle { color:#9aa9bf; font-size:13px; }
        QLabel#eyebrow { color:#a0afc7; font-size:10px; font-weight:600; letter-spacing:1.2px; }
        QLabel#navGroup { color:#74869f; font-size:9px; font-weight:600; letter-spacing:1.5px; padding:8px 13px; }
        QLabel#sectionTitle { font-size:14px; font-weight:600; color:#e9effb; }
        QLabel#value { font-size:30px; font-weight:600; letter-spacing:-0.8px; color:#f3f6ff; }
        QLabel#heroValue { font-size:26px; font-weight:600; color:#f3f6ff; }
        QLabel#welcomeTitle { font-size:22px; font-weight:600; letter-spacing:-0.4px; color:#eef2ff; }
        QLabel#note { color:#a0aec3; font-size:11px; }
        QLabel#micro { color:#8597b2; font-size:9px; letter-spacing:0.8px; }
        QLabel#rowValue { font-size:12px; font-weight:500; color:#e0e8f6; }
        QLabel#observation { color:#e8effc; font-size:19px; font-weight:550; }
        QLabel#live { color:#7fdfc1; background:#142b29; border:1px solid #254b43; border-radius:6px; padding:5px 9px; font-size:10px; letter-spacing:0.6px; }
        QLabel#privateBadge { color:#92d8c4; font-size:10px; font-weight:500; letter-spacing:0.6px; }
        QLabel#chartLegend { font-size:10px; letter-spacing:0.5px; }
        QLabel#badge { color:#b4c0ff; background:#2b3552; border-radius:5px; padding:4px 8px; font-size:10px; }
        QLabel#error { color:#efc28d; background:#302b24; border:1px solid #63503a; border-radius:8px; padding:11px; font-size:11px; }
        QPushButton { background:#252f43; border:1px solid #3d4c66; border-radius:8px; padding:10px 14px; color:#e5ecfa; }
        QPushButton:hover { background:#303d56; border-color:#788cb2; }
        QPushButton:pressed { background:#1c2639; }
        QPushButton:focus { border:1px solid #a1b1ff; }
        QPushButton:disabled { color:#708099; background:#1b2433; border-color:#2a364b; }
        QPushButton#primary { background:#8598ed; border-color:#a3b2ff; color:#10182c; font-weight:600; }
        QPushButton#primary:hover { background:#a6b6ff; }
        QPushButton#primary:pressed { background:#7185dc; }
        QPushButton#nav { text-align:left; padding:8px 13px; border:1px solid transparent; border-radius:8px; background:transparent; color:#a4b1c7; font-size:12px; }
        QPushButton#nav:hover { background:#1b2536; color:#eef3ff; }
        QPushButton#nav:checked { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #303c61,stop:1 #222e49); color:#c4ceff; border-color:#43547f; }
        QPushButton#nav:focus { border-color:#9aa9ff; }
        QPushButton#destructive { color:#edb4bc; }
        QComboBox { background:#1c2637; border:1px solid #35455f; border-radius:8px; padding:9px 14px; min-width:110px; color:#dce5f6; }
        QComboBox:hover { border-color:#667fa7; }
        QComboBox:focus { border-color:#9aa9ff; }
        QComboBox::drop-down { border:0; width:22px; }
        QComboBox QAbstractItemView { background:#202c40; color:#e7edfc; border:1px solid #435779; selection-background-color:#3b4b71; padding:5px; outline:0; }
        QLineEdit { background:#151e2d; border:1px solid #35455e; border-radius:9px; padding:11px 14px; selection-background-color:#485e94; }
        QLineEdit:focus { border-color:#9aa9ff; }
        QTableView,QTableWidget { background:#161e2c; alternate-background-color:#1a2434; border:1px solid #2c3b53; border-radius:9px; selection-background-color:#35466b; selection-color:#f1f5ff; outline:0; font-size:12px; }
        QTableView::item { padding:7px 11px; border-bottom:1px solid #242f43; }
        QTableView::item:selected { background:#35466b; }
        QTableView::item:hover { background:#26344e; }
        QHeaderView::section { background:#1d293c; color:#a7b6cd; border:0; border-bottom:1px solid #364963; padding:11px; font-size:10px; font-weight:600; }
        QTableCornerButton::section { background:#1d293c; border:0; }
        QTextEdit { background:transparent; border:0; color:#b6c5dc; font-size:12px; selection-background-color:#485e94; }
        QProgressBar { background:#2a374e; border:0; border-radius:4px; color:#edf2ff; font-size:10px; text-align:center; }
        QProgressBar::chunk { background:#8196e1; border-radius:4px; }
        QProgressBar#meter { background:#2a354a; border-radius:2px; }
        QProgressBar#meter::chunk { background:#9aa9ff; border-radius:2px; }
        QProgressBar#meter[tone="teal"]::chunk { background:#65d9c0; }
        QProgressBar#meter[tone="gold"]::chunk { background:#edc489; }
        QCheckBox { spacing:10px; padding:6px 0; }
        QCheckBox::indicator { width:17px; height:17px; background:#1c293d; border:1px solid #536c92; border-radius:5px; }
        QCheckBox::indicator:checked { background:#91a3f4; image:url(:/check.svg); }
        QCheckBox:focus { color:#c1ccff; }
        QScrollBar:vertical { background:transparent; width:7px; margin:2px 0; }
        QScrollBar::handle:vertical { background:#3b4b65; border-radius:3px; min-height:36px; }
        QScrollBar::handle:vertical:hover { background:#61779b; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
        QScrollBar:horizontal { background:#141e2d; height:7px; margin:0; }
        QScrollBar::handle:horizontal { background:#3b4b65; border-radius:3px; min-width:36px; }
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
        QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal { background:transparent; }
        QToolTip { background:#26354e; color:#ecf2ff; border:1px solid #566f96; padding:6px; }
        )");
        auto* root = new QWidget;
        root->setObjectName("root");
        auto* rl = new QHBoxLayout(root);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(0);
        auto* split = new QSplitter;
        split->setHandleWidth(1);
        split->setChildrenCollapsible(false);
        rl->addWidget(split);
        auto* sidebar = new QWidget;
        sidebar->setObjectName("sidebar");
        sidebar->setMinimumWidth(205);
        sidebar->setMaximumWidth(285);
        auto* sl = column(sidebar, 16);
        sidebarLayout = sl;
        sl->setContentsMargins(14, 24, 14, 18);
        sl->setSpacing(7);
        auto* brandRow = new QHBoxLayout;
        auto* mini = new CatWidget(true);
        cats.append(mini);
        brandRow->addWidget(mini);
        brandRow->addWidget(text("sentinel", "brand"));
        brandRow->addStretch();
        sl->addLayout(brandRow);
        auto* brandNote = text("YOUR MAC, IN FOCUS", "eyebrow");
        brandCaption = brandNote;
        brandNote->setContentsMargins(8, 0, 0, 18);
        sl->addWidget(brandNote);
        sl->addWidget(text("WORKSPACE", "navGroup"));
        const QStringList names = {"Dashboard",          "CPU",     "Memory",  "GPU",
                                   "Processes",          "Network", "Storage", "Security Center",
                                   "System Information", "Settings"};
        for (int i = 0; i < 9; ++i) {
            if (i == 7) {
                sl->addSpacing(14);
                sl->addWidget(text("SYSTEM", "navGroup"));
                sl->addSpacing(4);
            }
            auto* b = new QPushButton(names[i]);
            b->setObjectName("nav");
            b->setCheckable(true);
            b->setAccessibleName(names[i]);
            b->setCursor(Qt::PointingHandCursor);
            auto* shortcut = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), window);
            connect(shortcut, &QShortcut::activated, window, [this, i] { select(i); });
            b->setToolTip(names[i] + "  " + shortcut->key().toString(QKeySequence::NativeText));
            b->setIcon(icon(i));
            b->setIconSize(QSize(18, 18));
            b->setMinimumHeight(37);
            nav.append(b);
            sl->addWidget(b);
            connect(b, &QPushButton::clicked, window, [this, i] { select(i); });
        }
        sl->addStretch(1);
        auto* cat = new CatWidget;
        sidebarCat = cat;
        cats.append(cat);
        sl->addWidget(cat, 0, Qt::AlignLeft);
        auto* catNote = text("Keeping an eye on things.", "note");
        catCaption = catNote;
        catNote->setContentsMargins(9, 0, 0, 12);
        sl->addWidget(catNote);
        sl->addWidget(line());
        auto* settingsButton = new QPushButton("Settings");
        settingsButton->setObjectName("nav");
        settingsButton->setCheckable(true);
        settingsButton->setAccessibleName("Settings");
        settingsButton->setCursor(Qt::PointingHandCursor);
        settingsButton->setIcon(icon(9));
        settingsButton->setIconSize(QSize(18, 18));
        nav.append(settingsButton);
        sl->addWidget(settingsButton);
        connect(settingsButton, &QPushButton::clicked, window, [this] { select(9); });
        auto* version = text("LOCAL FIRST  /  SENTINEL " + QCoreApplication::applicationVersion(), "micro");
        version->setContentsMargins(13, 9, 0, 0);
        sl->addWidget(version);
        split->addWidget(sidebar);
        auto* content = new QWidget;
        auto* cl = column(content, 30);
        cl->setSpacing(20);
        auto* header = new QHBoxLayout;
        auto* titles = new QVBoxLayout;
        titles->setSpacing(7);
        title = text("Overview", "title");
        subtitle = text("A quiet, clear view of your Mac.", "subtitle");
        titles->addWidget(title);
        titles->addWidget(subtitle);
        header->addLayout(titles);
        header->addStretch();
        auto* tools = new QVBoxLayout;
        tools->setSpacing(10);
        live = text("●  CONNECTING", "live");
        live->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        tools->addWidget(live, 0, Qt::AlignRight);
        rangeChoice = new QComboBox;
        rangeChoice->addItem("Last 5 minutes", 300);
        rangeChoice->addItem("Last hour", 3600);
        rangeChoice->addItem("Last 24 hours", 86400);
        rangeChoice->setAccessibleName("Chart time range");
        tools->addWidget(rangeChoice);
        header->addLayout(tools);
        cl->addLayout(header);
        errorLabel = text("", "error");
        errorLabel->setWordWrap(true);
        errorLabel->hide();
        cl->addWidget(errorLabel);
        stack = new QStackedWidget;
        stack->installEventFilter(window);
        cl->addWidget(stack, 1);
        split->addWidget(content);
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setSizes({220, 1140});
        window->setCentralWidget(root);
        buildDashboard();
        buildCpu();
        buildMemory();
        buildGpu();
        buildProcesses();
        buildNetwork();
        buildStorage();
        buildSecurity();
        buildSystem();
        buildSettings();
        select(0);
        connect(rangeChoice, &QComboBox::currentIndexChanged, window, [this] {
            range = rangeChoice->currentData().toInt();
            service->requestHistory(range);
            updateCharts();
        });
        connect(service, &MonitorService::snapshotReady, window,
                [this](const Snapshot& s) { snapshotReceived(s); });
        connect(service, &MonitorService::systemInfoReady, window,
                [this](const SystemInfo& s) { infoReceived(s); });
        connect(service, &MonitorService::historyReady, window, [this](const QVector<HistoryPoint>& p) {
            // A database query must not erase the latest, not-yet-persisted samples.
            const qint64 newest = p.isEmpty() ? 0 : p.last().timestampMs;
            const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - qint64(range) * 1000;
            QVector<HistoryPoint> livePoints;
            for (const auto& point : history)
                if (point.bucketWidthMs == 0 && point.timestampMs > newest && point.timestampMs >= cutoff)
                    livePoints.append(point);
            history = p;
            history += livePoints;
            updateCharts();
        });
        connect(service, &MonitorService::securityReady, window,
                [this](const QVector<SecurityCheck>& checks) { securityReceived(checks); });
        connect(service, &MonitorService::connectionsReady, window,
                [this](const QVector<Connection>& list, const QString& error) {
                    connections->setRowCount(list.size());
                    for (int i = 0; i < list.size(); ++i) {
                        const auto& c = list[i];
                        tableRow(connections, i, {c.protocol, c.localAddress, c.remoteAddress, c.state});
                    }
                    connectionNote->setText(
                        error.isEmpty() ? QString("%1 accessible sockets for the selected process. This is a "
                                                  "point-in-time inspection; select it again to refresh.")
                                              .arg(list.size())
                                        : error);
                });
        connect(service, &MonitorService::signingReady, window, [this](const SecurityCheck& c) {
            QMessageBox box(QMessageBox::Information, c.name, c.status, QMessageBox::Ok, window);
            box.setInformativeText(c.detail);
            box.exec();
        });
        connect(service, &MonitorService::storageError, window, [this](const QString& e) {
            historyError = e;
            errorLabel->setText("History storage: " + e);
            errorLabel->show();
        });
        connect(service, &MonitorService::historyCleared, window, [this] {
            history.clear();
            updateCharts();
        });
        adjustSidebar(window->height());
        auto* find = new QShortcut(QKeySequence::Find, window);
        connect(find, &QShortcut::activated, window, [this] {
            select(4);
            processSearch->setFocus();
            processSearch->selectAll();
        });
    }
};
MainWindow::MainWindow(MonitorService* service, QWidget* parent)
    : QMainWindow(parent), d(std::make_unique<Impl>(this, service)) {
    d->build();
}
MainWindow::~MainWindow() = default;
void MainWindow::selectPage(int index) {
    d->select(index);
}
bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (d && watched == d->stack && event->type() == QEvent::Resize)
        d->adjustSidebar(height());
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (d)
        d->adjustSidebar(height());
}
} // namespace sentinel
