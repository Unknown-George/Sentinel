#include "ui/MainWindow.h"
#include "core/MonitorService.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace sentinel {
namespace {
const QColor blue("#8698ff"), teal("#67cdb3"), muted("#838b9f"), border("#2b2e3a");
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
    l->setSpacing(14);
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
class CatWidget final : public QWidget {
  public:
    explicit CatWidget(bool small = false) : small_(small) {
        setFixedSize(small ? QSize(32, 36) : QSize(146, 112));
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
        setAccessibleName("Sentinel black cat mascot");
        timer_.setInterval(80);
        connect(&timer_, &QTimer::timeout, this, [this] {
            ++tick_;
            const double target = hover_ ? 1.0 : 0.0;
            eyeOffset_ += (target - eyeOffset_) * 0.45;
            if (std::abs(target - eyeOffset_) < 0.005)
                eyeOffset_ = target;
            if (isVisible())
                update();
        });
        timer_.start();
    }
    void setMotion(bool on) {
        motion_ = on;
        if (on)
            timer_.start();
        else {
            timer_.stop();
            eyeOffset_ = 0.0;
        }
        update();
    }

  protected:
    void enterEvent(QEnterEvent* e) override {
        hover_ = true;
        update();
        QWidget::enterEvent(e);
    }
    void leaveEvent(QEvent* e) override {
        hover_ = false;
        update();
        QWidget::leaveEvent(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(width() / 146., height() / 112.);
        if (small_) {
            p.translate(-14, -8);
            p.scale(1.18, 1.18);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#0a0c12"));
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
        p.drawPath(body);
        QPainterPath tail;
        tail.moveTo(88, 92);
        double shift = motion_ ? std::sin(tick_ * .045) * 3 : 0;
        tail.cubicTo(124, 107, 140 + shift, 77, 123 + shift, 64);
        tail.cubicTo(114 + shift, 58, 106 + shift, 65, 112 + shift, 70);
        p.setPen(QPen(QColor("#0a0c12"), 10, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tail);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e7edf6"));
        bool blink = motion_ && (tick_ % 91 < 2);
        double dy = -eyeOffset_, dx = 2.0 * eyeOffset_;
        p.drawRoundedRect(QRectF(50 + dx, 47 + dy, 9, blink ? 1 : 5), 2, 2);
        p.drawRoundedRect(QRectF(76 + dx, 47 + dy, 9, blink ? 1 : 5), 2, 2);
    }

  private:
    bool small_ = false, motion_ = true, hover_ = false;
    int tick_ = 30;
    double eyeOffset_ = 0.0;
    QTimer timer_;
};
class HistoryChart final : public QWidget {
  public:
    enum Mode { Cpu, Memory, Network, Disk };
    explicit HistoryChart(Mode mode, QWidget* parent = nullptr) : QWidget(parent), mode_(mode) {
        setMinimumHeight(174);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    void setPoints(const QVector<HistoryPoint>& points, int range) {
        points_ = points;
        range_ = range;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot(4, 12, std::max(1, width() - (mode_ == Network ? 90 : 65)),
                          std::max(1, height() - 43));
        auto value = [this](const HistoryPoint& h, bool second) -> std::optional<double> {
            switch (mode_) {
            case Cpu:
                return h.cpuPercent;
            case Memory:
                return h.memoryTotal ? std::optional<double>(100. * h.memoryUsed / h.memoryTotal)
                                     : std::nullopt;
            case Network:
                return second ? h.uploadBytesPerSec : h.downloadBytesPerSec;
            case Disk:
                return h.diskTotal ? std::optional<double>(100. * h.diskUsed / h.diskTotal) : std::nullopt;
            }
            return std::nullopt;
        };
        const qint64 end = QDateTime::currentMSecsSinceEpoch(), start = end - qint64(range_) * 1000;
        double max = 100.;
        bool has = false;
        if (mode_ == Network) {
            max = 1024.;
            for (const auto& h : points_)
                if (h.timestampMs >= start) {
                    if (h.downloadBytesPerSec)
                        max = std::max(max, *h.downloadBytesPerSec * 1.15);
                    if (h.uploadBytesPerSec)
                        max = std::max(max, *h.uploadBytesPerSec * 1.15);
                }
        }
        p.setFont(QFont(".AppleSystemUIFont", 10));
        for (int i = 0; i <= 4; ++i) {
            double y = plot.top() + plot.height() * i / 4.;
            p.setPen(QPen(QColor("#2a2d39"), 1, Qt::DashLine));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            p.setPen(muted);
            QString label = mode_ == Network ? rate(max * (4 - i) / 4.)
                                             : QString::number(max * (4 - i) / 4., 'f', 0) + "%";
            p.drawText(QRectF(plot.right() + 7, y - 8, 82, 16), Qt::AlignVCenter, label);
        }
        for (int series = 0; series < (mode_ == Network ? 2 : 1); ++series) {
            QPainterPath path;
            bool connected = false;
            int segments = 0;
            qint64 previous = 0;
            qint64 previousBucketWidth = 0;
            for (const auto& h : points_) {
                if (h.timestampMs < start || h.timestampMs > end)
                    continue;
                auto v = value(h, series == 1);
                if (!v) {
                    connected = false;
                    continue;
                }
                has = true;
                QPointF point(plot.left() + double(h.timestampMs - start) / (end - start) * plot.width(),
                              plot.bottom() - std::clamp(*v / max, 0., 1.) * plot.height());
                const qint64 gapLimit =
                    std::max<qint64>(15000, 2 * std::max(previousBucketWidth, h.bucketWidthMs));
                if (!connected || h.timestampMs - previous > gapLimit) {
                    path.moveTo(point);
                    ++segments;
                } else
                    path.lineTo(point);
                connected = true;
                previous = h.timestampMs;
                previousBucketWidth = h.bucketWidthMs;
            }
            const QColor color = series ? teal : blue;
            // Never shade across missing samples or sleep/restart gaps.
            if (mode_ != Network && segments == 1 && !path.isEmpty()) {
                QPainterPath fill = path;
                fill.lineTo(path.currentPosition().x(), plot.bottom());
                fill.lineTo(path.elementAt(0).x, plot.bottom());
                fill.closeSubpath();
                QLinearGradient g(0, plot.top(), 0, plot.bottom());
                auto a = color;
                a.setAlpha(45);
                g.setColorAt(0, a);
                a.setAlpha(0);
                g.setColorAt(1, a);
                p.fillPath(fill, g);
            }
            p.setPen(QPen(color, 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
        }
        p.setPen(muted);
        for (int i = 0; i <= 3; ++i) {
            qint64 ts = start + (end - start) * i / 3;
            const QString s =
                QDateTime::fromMSecsSinceEpoch(ts).toString(range_ > 3600 ? "HH:mm" : "HH:mm:ss");
            double tickX = plot.left() + plot.width() * i / 3. - 35;
            if (i == 0)
                tickX = plot.left();
            else if (i == 3)
                tickX = plot.right() - 70;
            p.drawText(QRectF(tickX, plot.bottom() + 10, 70, 20),
                       i == 0   ? Qt::AlignLeft
                       : i == 3 ? Qt::AlignRight
                                : Qt::AlignHCenter,
                       s);
        }
        if (!has) {
            p.setPen(QColor("#a2a9bb"));
            p.drawText(plot, Qt::AlignCenter, "Waiting for measurements");
        }
    }

  private:
    Mode mode_;
    QVector<HistoryPoint> points_;
    int range_ = 300;
};
class ProcessModel final : public QAbstractTableModel {
  public:
    explicit ProcessModel(QObject* p) : QAbstractTableModel(p) {}
    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : items.size(); }
    int columnCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : 5; }
    QVariant headerData(int s, Qt::Orientation o, int role) const override {
        if (o == Qt::Horizontal && role == Qt::DisplayRole)
            return QStringList{"Process", "PID", "CPU %", "Memory", "User"}.value(s);
        return {};
    }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= items.size())
            return {};
        const auto& item = items[index.row()];
        int c = index.column();
        if (role == Qt::TextAlignmentRole)
            return int(c > 0 && c < 4 ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
        if (role == Qt::UserRole) {
            switch (c) {
            case 0:
                return item.name;
            case 1:
                return item.pid;
            case 2:
                return item.cpuPercent ? *item.cpuPercent : -1.;
            case 3:
                return QVariant::fromValue(item.residentBytes);
            case 4:
                return item.user;
            }
        }
        if (role == Qt::ToolTipRole)
            return item.path;
        if (role != Qt::DisplayRole)
            return {};
        switch (c) {
        case 0:
            return item.name;
        case 1:
            return item.pid;
        case 2:
            return percent(item.cpuPercent);
        case 3:
            return bytes(item.residentBytes);
        case 4:
            return item.user;
        }
        return {};
    }
    void replace(QVector<Process> p) {
        beginResetModel();
        items = std::move(p);
        endResetModel();
    }
    QVector<Process> items;
};
class ProcessFilter final : public QSortFilterProxyModel {
  public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

  protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        auto regex = filterRegularExpression();
        for (int c : {0, 1, 4})
            if (sourceModel()->index(row, c, parent).data().toString().contains(regex))
                return true;
        return false;
    }
};
QTableWidget* table(const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(false);
    t->setShowGrid(false);
    t->verticalHeader()->setDefaultSectionSize(37);
    return t;
}
void tableRow(QTableWidget* table, int row, const QStringList& values) {
    for (int col = 0; col < values.size(); ++col) {
        auto* item = table->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table->setItem(row, col, item);
        }
        item->setText(values[col]);
        item->setToolTip(values[col]);
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
    QVector<HistoryChart*> charts;
    QVector<CatWidget*> cats;
    QComboBox* rangeChoice = nullptr;
    QVector<HistoryPoint> history;
    Snapshot snapshot;
    SystemInfo info;
    int range = 300;
    bool haveSnapshot = false;
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
        if (labels.contains(key))
            labels[key]->setText(str);
    }
    QFrame* metric(const QString& key, const QString& name, const QString& hint, int glyph = 0) {
        auto* f = panel();
        auto* l = column(f, 18);
        auto* top = new QHBoxLayout;
        auto* label = text(name, "eyebrow");
        top->addWidget(label);
        top->addStretch();
        auto* i = text("");
        i->setPixmap(icon(glyph, true).pixmap(18, 18));
        top->addWidget(i);
        l->addLayout(top);
        l->addWidget(value(key));
        auto* hintLabel = value(key + "Hint", hint, "note");
        hintLabel->setWordWrap(true);
        l->addWidget(hintLabel);
        return f;
    }
    QWidget* page() {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body = new QWidget;
        body->setObjectName("pageBody");
        column(body, 0);
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
        h->addWidget(
            text(mode == HistoryChart::Network ? "↓ Download   ↑ Upload" : "MEASURED ACTIVITY", "eyebrow"));
        l->addLayout(h);
        auto* c = new HistoryChart(mode);
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
        auto* metrics = new QGridLayout;
        metrics->setSpacing(14);
        metrics->addWidget(metric("dashCpu", "CPU", "Measuring activity", 1), 0, 0);
        metrics->addWidget(metric("dashMemory", "MEMORY", "Physical memory", 2), 0, 1);
        metrics->addWidget(metric("dashDownload", "DOWNLOAD", "Physical Ethernet / Wi-Fi", 5), 0, 2);
        metrics->addWidget(metric("dashDisk", "STORAGE", "Startup volume", 6), 0, 3);
        l->addLayout(metrics);
        auto* hero = new QHBoxLayout;
        hero->setSpacing(14);
        hero->addWidget(chartPanel("CPU activity", HistoryChart::Cpu, 220), 3);
        auto* glance = panel();
        auto* gl = column(glance);
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
        lower->setSpacing(14);
        lower->addWidget(chartPanel("Memory footprint", HistoryChart::Memory, 130));
        auto* insight = panel();
        auto* il = column(insight);
        auto* it = new QHBoxLayout;
        it->addWidget(text("System observations", "sectionTitle"));
        it->addStretch();
        it->addWidget(text("LIVE", "badge"));
        il->addLayout(it);
        il->addWidget(value("observation", "Collecting the first system sample…", "observation"));
        il->addWidget(
            value("observationDetail", "Resource status will appear after measurements arrive.", "note"));
        labels["observationDetail"]->setWordWrap(true);
        il->addStretch();
        auto* b = new QPushButton("Open Security Center  →");
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
            detail->setPlainText("Select a process to inspect its details. Protected processes may expose "
                                 "only limited information.");
            return;
        }
        auto source = proxy->mapToSource(indices.front());
        if (source.row() >= processModel->items.size())
            return;
        const auto& p = processModel->items[source.row()];
        selectedPid = p.pid;
        selectedPath = p.path;
        detail->setPlainText(
            QString("%1\n\nPID  %2    •    User  %3\nCPU  %4    •    Resident memory  %5\n\n%6")
                .arg(p.name)
                .arg(p.pid)
                .arg(p.user, percent(p.cpuPercent), bytes(p.residentBytes),
                     p.path.isEmpty() ? "Executable path unavailable (process exited or access restricted)."
                                      : p.path));
    }
    void buildProcesses() {
        auto* p = page();
        auto* l = layout(p);
        auto* toolbar = new QHBoxLayout;
        auto* search = new QLineEdit;
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
                [this](const QString& q) { proxy->setFilterFixedString(q); });
        processTable = new QTableView;
        processTable->setObjectName("processTable");
        processTable->setModel(proxy);
        processTable->setSortingEnabled(true);
        processTable->sortByColumn(2, Qt::DescendingOrder);
        processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        processTable->setSelectionMode(QAbstractItemView::SingleSelection);
        processTable->setShowGrid(false);
        processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        processTable->verticalHeader()->hide();
        processTable->verticalHeader()->setDefaultSectionSize(37);
        processTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int c = 1; c < 5; ++c)
            processTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
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
        h->addWidget(refresh);
        connect(refresh, &QPushButton::clicked, window, [this] {
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
        auto* motion = new QCheckBox("Animate the Sentinel cat");
        motionEnabled = settings.value("animateMascot", true).toBool();
        motion->setChecked(motionEnabled);
        fl->addWidget(motion);
        fl->addWidget(note("Animations also respect the macOS Reduce Motion preference. Monitoring graphs "
                           "update only when a new measurement arrives."));
        connect(motion, &QCheckBox::toggled, window, [this](bool b) {
            motionEnabled = b;
            settings.setValue("animateMascot", b);
            for (auto* cat : cats)
                cat->setMotion(b && !info.reducedMotion);
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
        l->addWidget(note("Sentinel 1.0 • C++20 / Qt 6 • Designed for Apple Silicon and macOS 14 or later."));
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
        sidebarCat->setVisible(height >= 740);
        sidebarCat->setFixedSize(height < 820 ? QSize(91, 70) : QSize(146, 112));
        for (auto* label : labels)
            if (label->objectName() == "value")
                label->setStyleSheet(window->width() < 1150 ? "font-size:24px;" : "font-size:30px;");
    }
    void select(int i) {
        if (i < 0 || i >= stack->count())
            return;
        stack->setCurrentIndex(i);
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
        rangeChoice->setVisible(i <= 3 || i == 5 || i == 6);
        if (i == 7)
            service->requestSecurity();
    }
    void updateCharts() {
        for (auto* c : charts)
            c->setPoints(history, range);
    }
    void snapshotReceived(const Snapshot& s) {
        snapshot = s;
        haveSnapshot = true;
        ++sampleCount;
        live->setText("●  LIVE  ·  " + s.timestamp.toLocalTime().toString("HH:mm:ss"));
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
        int keepPid = selectedPid;
        processModel->replace(s.processes);
        if (keepPid) {
            for (int row = 0; row < processModel->items.size(); ++row)
                if (processModel->items[row].pid == keepPid) {
                    auto idx = proxy->mapFromSource(processModel->index(row, 0));
                    if (idx.isValid())
                        processTable->selectRow(idx.row());
                    break;
                }
        }
        processCount->setText(QString("%1 visible processes").arg(s.processes.size()));
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
        for (auto* cat : cats)
            cat->setMotion(motionEnabled && !s.reducedMotion);
    }
    void securityReceived(const QVector<SecurityCheck>& checks) {
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
        window->resize(1280, 900);
        window->setMinimumSize(1000, 700);
        window->setUnifiedTitleAndToolBarOnMac(false);
        window->setStyleSheet(R"(
        QMainWindow,QWidget#root { background:#14161e; color:#e8ebf4; }
        QWidget { color:#dce0ec; font-family:".AppleSystemUIFont","SF Pro Text","Helvetica Neue"; font-size:13px; }
        QWidget#sidebar { background:#191b24; border-right:1px solid #2b2e3a; }
        QWidget#pageBody,QScrollArea,QScrollArea>QWidget>QWidget { background:#14161e; }
        QFrame#panel { background:#1c1f2a; border:1px solid #2c303d; border-radius:12px; }
        QFrame#separator { background:#2c303d; border:0; }
        QLabel { background:transparent; border:0; }
        QLabel#brand { font-size:22px; font-weight:650; letter-spacing:1px; }
        QLabel#title { font-size:29px; font-weight:650; letter-spacing:-0.7px; }
        QLabel#subtitle { color:#858da1; font-size:13px; }
        QLabel#eyebrow { color:#9199ae; font-size:10px; font-weight:600; letter-spacing:1.0px; }
        QLabel#sectionTitle { font-size:14px; font-weight:600; color:#e7ebf5; }
        QLabel#value { font-size:30px; font-weight:550; letter-spacing:-0.7px; color:#f2f4fa; }
        QLabel#heroValue { font-size:25px; font-weight:550; color:#f2f4fa; }
        QLabel#note { color:#9199ad; font-size:11px; line-height:1.5; }
        QLabel#rowValue { font-size:12px; font-weight:500; color:#dce1ef; }
        QLabel#observation { color:#e3e7f2; font-size:19px; font-weight:550; }
        QLabel#live { color:#81cbb3; font-size:10px; letter-spacing:0.8px; }
        QLabel#badge { color:#93a2fa; background:#292e43; border-radius:5px; padding:4px 7px; font-size:10px; }
        QLabel#error { color:#edbd85; background:#302920; border:1px solid #584632; border-radius:7px; padding:10px; font-size:11px; }
        QPushButton { background:#292e3e; border:1px solid #3a4155; border-radius:7px; padding:9px 13px; color:#dce2f3; }
        QPushButton:hover { background:#32394d; border-color:#596585; }
        QPushButton:pressed { background:#202637; }
        QPushButton:focus { border-color:#8998ff; }
        QPushButton#nav { text-align:left; padding:8px 13px; border:1px solid transparent; border-radius:7px; background:transparent; color:#9da5b8; font-size:12px; }
        QPushButton#nav:hover { background:#232735; color:#dce2f3; }
        QPushButton#nav:checked { background:#2b3047; color:#aab5ff; border-color:#3a415e; }
        QPushButton#destructive { color:#e0a7aa; }
        QComboBox { background:#222633; border:1px solid #353b4e; border-radius:7px; padding:8px 13px; min-width:105px; }
        QComboBox::drop-down { border:0; width:22px; }
        QComboBox QAbstractItemView { background:#242938; selection-background-color:#394360; }
        QLineEdit { background:#1c202c; border:1px solid #343b4e; border-radius:8px; padding:10px 13px; selection-background-color:#48578d; }
        QLineEdit:focus { border-color:#8595ef; }
        QTableView,QTableWidget { background:#1b1e29; border:1px solid #2c3241; border-radius:8px; selection-background-color:#303a55; selection-color:#e9edfa; outline:0; font-size:12px; }
        QTableView::item { padding:6px 10px; border-bottom:1px solid #282d3a; }
        QTableView::item:selected { background:#303a55; }
        QHeaderView::section { background:#222633; color:#8f98ae; border:0; border-bottom:1px solid #343b4a; padding:10px; font-size:10px; }
        QTableCornerButton::section { background:#222633; border:0; }
        QTextEdit { background:transparent; border:0; color:#aeb7cc; font-size:12px; selection-background-color:#48578d; }
        QProgressBar { background:#2b3041; border:0; border-radius:4px; color:#e5e9f5; font-size:10px; text-align:center; }
        QProgressBar::chunk { background:#7188d8; border-radius:4px; }
        QCheckBox { spacing:9px; padding:5px 0; }
        QCheckBox::indicator { width:16px; height:16px; background:#252b3a; border:1px solid #4b5671; border-radius:4px; }
        QCheckBox::indicator:checked { background:#7c8de3; image:url(:/check.svg); }
        QScrollBar:vertical { background:transparent; width:8px; margin:0; }
        QScrollBar::handle:vertical { background:#3b4255; border-radius:4px; min-height:30px; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
        QToolTip { background:#292e40; border:1px solid #47516a; color:#e1e6f3; padding:5px; }
        QMessageBox,QDialog { background:#202431; }
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
        brandNote->setContentsMargins(8, 0, 0, 20);
        sl->addWidget(brandNote);
        const QStringList names = {"Dashboard",          "CPU",     "Memory",  "GPU",
                                   "Processes",          "Network", "Storage", "Security Center",
                                   "System Information", "Settings"};
        for (int i = 0; i < 9; ++i) {
            if (i == 7) {
                sl->addSpacing(14);
                sl->addWidget(text("SYSTEM", "eyebrow"));
                sl->addSpacing(4);
            }
            auto* b = new QPushButton(names[i]);
            b->setObjectName("nav");
            b->setCheckable(true);
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
        settingsButton->setIcon(icon(9));
        settingsButton->setIconSize(QSize(18, 18));
        nav.append(settingsButton);
        sl->addWidget(settingsButton);
        connect(settingsButton, &QPushButton::clicked, window, [this] { select(9); });
        auto* version = text("SENTINEL  1.0", "eyebrow");
        version->setContentsMargins(13, 9, 0, 0);
        sl->addWidget(version);
        split->addWidget(sidebar);
        auto* content = new QWidget;
        auto* cl = column(content, 27);
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
        cl->addWidget(stack, 1);
        split->addWidget(content);
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setSizes({218, 1062});
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
            history = p;
            if (haveSnapshot &&
                snapshot.timestamp.toMSecsSinceEpoch() >
                    (history.isEmpty() ? 0 : history.last().timestampMs)) { /* next live sample will append */
            }
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
        QTimer::singleShot(0, window, [this] {
            service->requestHistory(range);
            service->requestSecurity();
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
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (d)
        d->adjustSidebar(height());
}
} // namespace sentinel
