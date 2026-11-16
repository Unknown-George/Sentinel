#include "platform/Collector.h"
#include "core/MetricsMath.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <Metal/Metal.h>
#import <Security/Security.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QSysInfo>
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <dispatch/dispatch.h>
#include <ifaddrs.h>
#include <libproc.h>
#include <limits>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <memory>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <vector>

namespace sentinel {
namespace {
using Clock = std::chrono::steady_clock;

template <class T> std::optional<T> sysValue(const char* name) {
    T value{};
    size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0 || size != sizeof(value))
        return {};
    return value;
}
QString sysString(const char* name) {
    size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0 || size > 65536)
        return {};
    QByteArray data(static_cast<qsizetype>(size), '\0');
    if (sysctlbyname(name, data.data(), &size, nullptr, 0) != 0)
        return {};
    return QString::fromUtf8(data.constData());
}
QString nsString(NSString* string) {
    return string ? QString::fromUtf8(string.UTF8String) : QString();
}
QString cfString(CFStringRef string) {
    if (!string || CFGetTypeID(string) != CFStringGetTypeID())
        return {};
    return nsString((__bridge NSString*)string);
}
QString osError(OSStatus status) {
    CFStringRef text = SecCopyErrorMessageString(status, nullptr);
    const QString result = text ? cfString(text) : QString::number(status);
    if (text)
        CFRelease(text);
    return result;
}
struct CommandResult {
    bool ok = false;
    QString output;
};
CommandResult runReadOnly(const QString& executable, const QStringList& arguments) {
    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(700))
        return {false, process.errorString()};
    if (!process.waitForFinished(2200)) {
        process.kill();
        process.waitForFinished(500);
        return {false, "The system command timed out."};
    }
    const QString output = QString::fromUtf8(process.readAll()).trimmed().left(4096);
    return {process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, output};
}
QString userName(uid_t uid) {
    struct passwd record{};
    struct passwd* result = nullptr;
    std::array<char, 16384> buffer{};
    if (getpwuid_r(uid, &record, buffer.data(), buffer.size(), &result) == 0 && result)
        return QString::fromUtf8(record.pw_name);
    return QString::number(uid);
}
QString processPath(int pid) {
    std::array<char, PROC_PIDPATHINFO_MAXSIZE> path{};
    if (proc_pidpath(pid, path.data(), static_cast<uint32_t>(path.size())) <= 0)
        return {};
    return QString::fromUtf8(path.data());
}
struct InterfaceCounters {
    quint64 received = 0, sent = 0;
    unsigned char type = 0;
};
QHash<QString, InterfaceCounters> interfaceCounters(bool* ok) {
    // NET_RT_IFLIST2 exposes public, 64-bit counters; getifaddrs' if_data may wrap at 4 GiB.
    int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
    size_t size = 0;
    QHash<QString, InterfaceCounters> result;
    *ok = false;
    if (sysctl(mib, 6, nullptr, &size, nullptr, 0) != 0 || size > 16 * 1024 * 1024)
        return result;
    std::vector<char> data(size);
    if (sysctl(mib, 6, data.data(), &size, nullptr, 0) != 0)
        return result;
    // Address messages interleaved in this stream are smaller than if_msghdr.
    // Only the four-byte routing message prefix is common to every entry.
    constexpr size_t prefixSize = sizeof(uint16_t) + 2 * sizeof(uint8_t);
    for (size_t offset = 0; offset + prefixSize <= size;) {
        const auto* header = reinterpret_cast<const if_msghdr*>(data.data() + offset);
        if (header->ifm_msglen < prefixSize || offset + header->ifm_msglen > size)
            break;
        if (header->ifm_type == RTM_IFINFO2 && header->ifm_msglen >= sizeof(if_msghdr2)) {
            const auto* info = reinterpret_cast<const if_msghdr2*>(header);
            char name[IF_NAMESIZE]{};
            if (if_indextoname(info->ifm_index, name)) {
                result.insert(QString::fromUtf8(name), {info->ifm_data.ifi_ibytes, info->ifm_data.ifi_obytes,
                                                        info->ifm_data.ifi_type});
            }
        }
        offset += header->ifm_msglen;
    }
    *ok = true;
    return result;
}
QString socketEndpoint(const in_sockinfo& socket, bool remote) {
    std::array<char, INET6_ADDRSTRLEN> address{};
    const bool ipv6 = (socket.insi_vflag & INI_IPV6) != 0;
    const void* raw =
        ipv6 ? static_cast<const void*>(remote ? &socket.insi_faddr.ina_6 : &socket.insi_laddr.ina_6)
             : static_cast<const void*>(remote ? &socket.insi_faddr.ina_46.i46a_addr4
                                               : &socket.insi_laddr.ina_46.i46a_addr4);
    if (!inet_ntop(ipv6 ? AF_INET6 : AF_INET, raw, address.data(), address.size()))
        return "Unavailable";
    const quint16 port = ntohs(static_cast<uint16_t>(remote ? socket.insi_fport : socket.insi_lport));
    const QString host = QString::fromLatin1(address.data());
    return ipv6 ? QString("[%1]:%2").arg(host).arg(port) : QString("%1:%2").arg(host).arg(port);
}
QString tcpState(int state) {
    static const std::array<const char*, 12> names = {
        "Closed",     "Listen",  "SYN sent", "SYN received", "Established", "Close wait",
        "FIN wait 1", "Closing", "Last ACK", "FIN wait 2",   "Time wait",   "Reserved"};
    return state >= 0 && state < static_cast<int>(names.size()) ? names[static_cast<size_t>(state)]
                                                                : "Unknown";
}
} // namespace

struct Collector::Impl {
    host_t host = mach_host_self();
    mach_timebase_info_data_t timebase{};
    std::vector<CpuTicks> previousCpu;
    struct ProcessBaseline {
        quint64 start = 0, cpuNs = 0;
        Clock::time_point time;
    };
    QHash<int, ProcessBaseline> previousProcesses;
    struct NetworkBaseline {
        quint64 received = 0, sent = 0;
        Clock::time_point time;
    };
    QHash<QString, NetworkBaseline> previousNetwork;
    QHash<uid_t, QString> users;
    std::shared_ptr<std::atomic<unsigned long>> pressure = std::make_shared<std::atomic<unsigned long>>(0);
    dispatch_source_t pressureSource = nullptr;
    SystemInfo info;
    Impl() {
        mach_timebase_info(&timebase);
        const auto state = pressure;
        pressureSource = dispatch_source_create(
            DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0,
            DISPATCH_MEMORYPRESSURE_NORMAL | DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL,
            dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
        if (pressureSource) {
            __weak dispatch_source_t source = pressureSource;
            dispatch_source_set_event_handler(pressureSource, ^{
              dispatch_source_t liveSource = source;
              if (liveSource)
                  state->store(dispatch_source_get_data(liveSource), std::memory_order_relaxed);
            });
            dispatch_resume(pressureSource);
        }
    }
    ~Impl() {
        if (pressureSource)
            dispatch_source_cancel(pressureSource);
        if (host != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), host);
    }
};

Collector::Collector() : d(std::make_unique<Impl>()) {
    d->info = systemInfo();
}
Collector::~Collector() = default;

SystemInfo Collector::systemInfo() {
    @autoreleasepool {
        SystemInfo info;
        info.cpuModel = sysString("machdep.cpu.brand_string");
        info.model = sysString("hw.model");
        info.architecture = QSysInfo::currentCpuArchitecture();
        info.osVersion = nsString(NSProcessInfo.processInfo.operatingSystemVersionString);
        info.hostname = nsString(NSProcessInfo.processInfo.hostName);
        info.logicalCores = sysValue<int>("hw.logicalcpu").value_or(0);
        info.physicalCores = sysValue<int>("hw.physicalcpu").value_or(0);
        info.physicalMemory = sysValue<uint64_t>("hw.memsize").value_or(0);
        info.reducedMotion = NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
        // IOPlatformExpertDevice's public registry model property is a fallback, not a serial-number probe.
        if (info.model.isEmpty()) {
            io_service_t platform =
                IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
            if (platform) {
                CFTypeRef model =
                    IORegistryEntryCreateCFProperty(platform, CFSTR("model"), kCFAllocatorDefault, 0);
                if (model && CFGetTypeID(model) == CFDataGetTypeID()) {
                    const auto data = static_cast<CFDataRef>(model);
                    info.model = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                                                   CFDataGetLength(data));
                    info.model.remove(QChar('\0'));
                }
                if (model)
                    CFRelease(model);
                IOObjectRelease(platform);
            }
        }
        id<MTLDevice> gpu = MTLCreateSystemDefaultDevice();
        if (gpu) {
            info.gpuName = nsString(gpu.name);
            info.gpuUnifiedMemory = gpu.hasUnifiedMemory;
            info.gpuRecommendedWorkingSet = gpu.recommendedMaxWorkingSetSize;
        }
        return info;
    }
}

Snapshot Collector::collect() {
    @autoreleasepool {
        Snapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTimeUtc();
        const auto now = Clock::now();
        natural_t cpuCount = 0;
        processor_info_array_t rawCpu = nullptr;
        mach_msg_type_number_t cpuInfoCount = 0;
        if (host_processor_info(d->host, PROCESSOR_CPU_LOAD_INFO, &cpuCount, &rawCpu, &cpuInfoCount) ==
            KERN_SUCCESS) {
            const auto* loads = reinterpret_cast<processor_cpu_load_info_t>(rawCpu);
            std::vector<CpuTicks> current;
            current.reserve(cpuCount);
            bool allValid = d->previousCpu.size() == cpuCount;
            CpuTicks aggregatePrevious{}, aggregateCurrent{};
            for (natural_t i = 0; i < cpuCount; ++i) {
                const CpuTicks ticks{static_cast<uint32_t>(loads[i].cpu_ticks[CPU_STATE_USER]),
                                     static_cast<uint32_t>(loads[i].cpu_ticks[CPU_STATE_SYSTEM]),
                                     static_cast<uint32_t>(loads[i].cpu_ticks[CPU_STATE_IDLE]),
                                     static_cast<uint32_t>(loads[i].cpu_ticks[CPU_STATE_NICE])};
                current.push_back(ticks);
                if (d->previousCpu.size() == cpuCount) {
                    const auto percent = cpuUsage(d->previousCpu[i], ticks);
                    if (!percent)
                        allValid = false;
                    snapshot.corePercent.append(percent.value_or(0.0));
                    const auto& previous = d->previousCpu[i];
                    aggregatePrevious.user += previous.user;
                    aggregatePrevious.system += previous.system;
                    aggregatePrevious.idle += previous.idle;
                    aggregatePrevious.nice += previous.nice;
                    aggregateCurrent.user += ticks.user;
                    aggregateCurrent.system += ticks.system;
                    aggregateCurrent.idle += ticks.idle;
                    aggregateCurrent.nice += ticks.nice;
                }
            }
            if (allValid)
                snapshot.cpuPercent = cpuUsage(aggregatePrevious, aggregateCurrent);
            else
                snapshot.corePercent.clear(); // A reset is missing data, never a zero-percent reading.
            d->previousCpu = std::move(current);
            vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(rawCpu),
                          cpuInfoCount * sizeof(integer_t));
        } else
            snapshot.errors << "CPU counters are unavailable from Mach.";

        vm_statistics64_data_t vm{};
        mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
        vm_size_t pageSize = 0;
        snapshot.memoryTotal = d->info.physicalMemory;
        if (host_page_size(d->host, &pageSize) == KERN_SUCCESS &&
            host_statistics64(d->host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &vmCount) ==
                KERN_SUCCESS &&
            pageSize > 0 && snapshot.memoryTotal > 0) {
            snapshot.memoryValid = true;
            snapshot.memoryWired = static_cast<quint64>(vm.wire_count) * pageSize;
            snapshot.memoryCompressed = static_cast<quint64>(vm.compressor_page_count) * pageSize;
            snapshot.memoryActive = static_cast<quint64>(vm.active_count) * pageSize;
            snapshot.memoryInactive = static_cast<quint64>(vm.inactive_count) * pageSize;
            snapshot.memoryFree = static_cast<quint64>(vm.free_count) * pageSize;
            snapshot.memoryPurgeable = static_cast<quint64>(vm.purgeable_count) * pageSize;
            const quint64 internal = static_cast<quint64>(vm.internal_page_count) * pageSize;
            const quint64 app = internal > snapshot.memoryPurgeable ? internal - snapshot.memoryPurgeable : 0;
            snapshot.memoryUsed =
                std::min(snapshot.memoryTotal, snapshot.memoryWired + snapshot.memoryCompressed + app);
            snapshot.memoryAvailable = snapshot.memoryTotal - snapshot.memoryUsed;
        } else
            snapshot.errors << "Virtual-memory counters are unavailable from Mach.";
        if (const auto swap = sysValue<xsw_usage>("vm.swapusage")) {
            snapshot.swapUsed = swap->xsu_used;
            snapshot.swapTotal = swap->xsu_total;
            snapshot.swapValid = true;
        } else
            snapshot.errors << "Swap usage is unavailable.";
        const auto pressure = d->pressure->load(std::memory_order_relaxed);
        if (pressure & DISPATCH_MEMORYPRESSURE_CRITICAL)
            snapshot.memoryPressure = "Critical";
        else if (pressure & DISPATCH_MEMORYPRESSURE_WARN)
            snapshot.memoryPressure = "Warning";
        else if (pressure & DISPATCH_MEMORYPRESSURE_NORMAL)
            snapshot.memoryPressure = "Normal";
        if (const auto boot = sysValue<timeval>("kern.boottime")) {
            const qint64 uptime = snapshot.timestamp.toSecsSinceEpoch() - boot->tv_sec;
            snapshot.uptimeSeconds = static_cast<quint64>(std::max<qint64>(0, uptime));
            snapshot.uptimeValid = true;
        } else
            snapshot.errors << "System uptime is unavailable.";

        const int processCount = proc_listallpids(nullptr, 0);
        if (processCount > 0 && processCount < 1000000) {
            std::vector<pid_t> pids(static_cast<size_t>(processCount) + 128);
            const int count = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
            QHash<int, Impl::ProcessBaseline> baselines;
            int skipped = 0;
            for (int index = 0; index < std::min(count, static_cast<int>(pids.size())); ++index) {
                const int pid = pids[static_cast<size_t>(index)];
                if (pid <= 0)
                    continue;
                proc_taskallinfo task{};
                if (proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &task, sizeof(task)) != sizeof(task)) {
                    ++skipped;
                    continue;
                }
                Process process;
                process.pid = pid;
                process.name =
                    QString::fromUtf8(task.pbsd.pbi_name[0] ? task.pbsd.pbi_name : task.pbsd.pbi_comm);
                process.path = processPath(pid);
                if (process.name.isEmpty())
                    process.name = QFileInfo(process.path).fileName();
                if (process.name.isEmpty())
                    process.name = QString("Process %1").arg(pid);
                if (!d->users.contains(task.pbsd.pbi_uid))
                    d->users.insert(task.pbsd.pbi_uid, userName(task.pbsd.pbi_uid));
                process.user = d->users.value(task.pbsd.pbi_uid);
                process.residentBytes = task.ptinfo.pti_resident_size;
                process.startTime = task.pbsd.pbi_start_tvsec * 1000000ULL + task.pbsd.pbi_start_tvusec;
                // libproc's task CPU totals are Mach absolute ticks on Apple Silicon, not nanoseconds.
                const long double ticks =
                    static_cast<long double>(task.ptinfo.pti_total_user) + task.ptinfo.pti_total_system;
                const long double nanos =
                    d->timebase.denom ? ticks * d->timebase.numer / d->timebase.denom : 0;
                const quint64 cpuNs = static_cast<quint64>(
                    std::min(nanos, static_cast<long double>(std::numeric_limits<quint64>::max())));
                const auto previous = d->previousProcesses.constFind(pid);
                if (d->timebase.denom && previous != d->previousProcesses.cend() &&
                    previous->start == process.startTime)
                    process.cpuPercent = processCpuUsage(
                        previous->cpuNs, cpuNs, std::chrono::duration<double>(now - previous->time).count());
                baselines.insert(pid, {process.startTime, cpuNs, now});
                snapshot.processes.append(std::move(process));
            }
            d->previousProcesses = std::move(baselines);
            if (skipped)
                snapshot.notices << QString(
                                        "%1 processes exited during collection or are protected by macOS; "
                                        "only readable processes are listed.")
                                        .arg(skipped);
            if (count < 0)
                snapshot.errors << "The process list is unavailable.";
        } else
            snapshot.errors << "The process list is unavailable.";

        bool countersValid = false;
        const auto counters = interfaceCounters(&countersValid);
        QHash<QString, NetworkInterface> interfaces;
        ifaddrs* addresses = nullptr;
        if (getifaddrs(&addresses) == 0) {
            for (const ifaddrs* item = addresses; item; item = item->ifa_next) {
                if (!item->ifa_name || !item->ifa_addr)
                    continue;
                const QString name = QString::fromUtf8(item->ifa_name);
                auto& interface = interfaces[name];
                interface.name = name;
                interface.up = (item->ifa_flags & IFF_UP) && (item->ifa_flags & IFF_RUNNING);
                interface.loopback = (item->ifa_flags & IFF_LOOPBACK) != 0;
                std::array<char, INET6_ADDRSTRLEN> address{};
                const int family = item->ifa_addr->sa_family;
                const void* raw = family == AF_INET
                                      ? static_cast<const void*>(
                                            &reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr)
                                  : family == AF_INET6
                                      ? static_cast<const void*>(
                                            &reinterpret_cast<const sockaddr_in6*>(item->ifa_addr)->sin6_addr)
                                      : nullptr;
                if (raw && inet_ntop(family, raw, address.data(), address.size())) {
                    if (!interface.address.isEmpty())
                        interface.address += ", ";
                    interface.address += QString::fromLatin1(address.data());
                }
            }
            freeifaddrs(addresses);
        } else
            snapshot.errors << "Network-interface addresses are unavailable.";
        QHash<QString, Impl::NetworkBaseline> networkBaselines;
        bool aggregateReady = countersValid;
        int aggregateCount = 0;
        double download = 0, upload = 0;
        QStringList names = interfaces.keys();
        names.sort();
        for (const auto& name : names) {
            NetworkInterface interface = interfaces.value(name);
            const auto counter = counters.constFind(name);
            if (counter != counters.cend()) {
                interface.receivedBytes = counter->received;
                interface.sentBytes = counter->sent;
                const auto previous = d->previousNetwork.constFind(name);
                if (previous != d->previousNetwork.cend()) {
                    const double seconds = std::chrono::duration<double>(now - previous->time).count();
                    interface.downloadBytesPerSec =
                        counterRate(previous->received, counter->received, seconds);
                    interface.uploadBytesPerSec = counterRate(previous->sent, counter->sent, seconds);
                }
                networkBaselines.insert(name, {counter->received, counter->sent, now});
                // Apple's physical Ethernet/Wi-Fi interfaces are en*. Exclude tunnels/bridges to avoid
                // counting VPN traffic twice.
                if (interface.up && !interface.loopback && name.startsWith("en") &&
                    counter->type == IFT_ETHER) {
                    ++aggregateCount;
                    if (interface.downloadBytesPerSec && interface.uploadBytesPerSec) {
                        download += *interface.downloadBytesPerSec;
                        upload += *interface.uploadBytesPerSec;
                    } else
                        aggregateReady = false;
                }
            }
            snapshot.interfaces.append(std::move(interface));
        }
        d->previousNetwork = std::move(networkBaselines);
        if (aggregateReady && aggregateCount > 0) {
            snapshot.downloadBytesPerSec = download;
            snapshot.uploadBytesPerSec = upload;
        }
        if (!countersValid)
            snapshot.errors << "64-bit network counters are unavailable.";
        else if (aggregateCount == 0)
            snapshot.errors
                << "No active physical Ethernet/Wi-Fi interface is available for aggregate network rates.";

        struct statfs* mounts = nullptr;
        const int mountCount = getmntinfo(&mounts, MNT_NOWAIT);
        for (int i = 0; i < mountCount; ++i) {
            const auto& fs = mounts[i];
            if (fs.f_blocks == 0 || fs.f_bsize == 0 || std::strcmp(fs.f_fstypename, "devfs") == 0)
                continue;
            Volume volume;
            volume.name = QString::fromUtf8(fs.f_mntfromname);
            volume.mountPoint = QString::fromUtf8(fs.f_mntonname);
            volume.filesystem = QString::fromUtf8(fs.f_fstypename);
            volume.totalBytes = fs.f_blocks * static_cast<quint64>(fs.f_bsize);
            volume.availableBytes =
                std::min(volume.totalBytes, fs.f_bavail * static_cast<quint64>(fs.f_bsize));
            volume.readOnly = (fs.f_flags & MNT_RDONLY) != 0;
            snapshot.volumes.append(std::move(volume));
        }
        if (mountCount <= 0)
            snapshot.errors << "Mounted-volume statistics are unavailable.";
        return snapshot;
    }
}

QVector<SecurityCheck> Collector::securityChecks() {
    QVector<SecurityCheck> checks;
    const auto firewall =
        runReadOnly("/usr/libexec/ApplicationFirewall/socketfilterfw", {"--getglobalstate"});
    SecurityCheck firewallCheck{"Application firewall", "Unknown", firewall.output, "unknown"};
    if (firewall.ok && firewall.output.contains("disabled", Qt::CaseInsensitive)) {
        firewallCheck.status = "Disabled";
        firewallCheck.level = "disabled";
    } else if (firewall.ok && firewall.output.contains("enabled", Qt::CaseInsensitive)) {
        firewallCheck.status = "Enabled";
        firewallCheck.level = "enabled";
    }
    checks.append(firewallCheck);
    const auto fileVault = runReadOnly("/usr/bin/fdesetup", {"status"});
    SecurityCheck vaultCheck{"FileVault", "Unknown", fileVault.output, "unknown"};
    if (fileVault.ok && fileVault.output.contains("FileVault is On", Qt::CaseInsensitive)) {
        vaultCheck.status = "Enabled";
        vaultCheck.level = "enabled";
    } else if (fileVault.ok && fileVault.output.contains("FileVault is Off", Qt::CaseInsensitive)) {
        vaultCheck.status = "Disabled";
        vaultCheck.level = "disabled";
    } else if (fileVault.ok && fileVault.output.contains("progress", Qt::CaseInsensitive))
        vaultCheck.status = "In progress";
    checks.append(vaultCheck);
    const auto gatekeeper = runReadOnly("/usr/sbin/spctl", {"--status"});
    SecurityCheck gatekeeperCheck{"Gatekeeper", "Unknown", gatekeeper.output, "unknown"};
    if (gatekeeper.ok && gatekeeper.output.contains("assessments enabled", Qt::CaseInsensitive)) {
        gatekeeperCheck.status = "Enabled";
        gatekeeperCheck.level = "enabled";
    } else if (gatekeeper.ok && gatekeeper.output.contains("assessments disabled", Qt::CaseInsensitive)) {
        gatekeeperCheck.status = "Disabled";
        gatekeeperCheck.level = "disabled";
    }
    checks.append(gatekeeperCheck);
    auto signing = signingInfo(QCoreApplication::applicationFilePath());
    signing.name = "Sentinel code signature";
    checks.append(signing);
    return checks;
}

SecurityCheck Collector::signingInfo(const QString& executablePath) {
    @autoreleasepool {
        SecurityCheck check{"Code signature", "Unknown", {}, "unknown"};
        if (executablePath.isEmpty() || !QFileInfo::exists(executablePath)) {
            check.detail = "The executable path is unavailable or the process has exited.";
            return check;
        }
        NSURL* url = [NSURL fileURLWithPath:executablePath.toNSString()];
        SecStaticCodeRef code = nullptr;
        OSStatus status = SecStaticCodeCreateWithPath((__bridge CFURLRef)url, kSecCSDefaultFlags, &code);
        if (status != errSecSuccess || !code) {
            check.detail = osError(status);
            return check;
        }
        status =
            SecStaticCodeCheckValidity(code, kSecCSStrictValidate | kSecCSCheckAllArchitectures, nullptr);
        if (status == errSecSuccess) {
            check.status = "Valid signature";
            check.level = "info";
            check.detail = "Static code signature is valid. This does not establish notarization, origin "
                           "trust, or absence of malware.";
            CFDictionaryRef signing = nullptr;
            if (SecCodeCopySigningInformation(code, kSecCSSigningInformation, &signing) == errSecSuccess &&
                signing) {
                const QString identifier =
                    cfString(static_cast<CFStringRef>(CFDictionaryGetValue(signing, kSecCodeInfoIdentifier)));
                const QString team = cfString(
                    static_cast<CFStringRef>(CFDictionaryGetValue(signing, kSecCodeInfoTeamIdentifier)));
                if (!identifier.isEmpty())
                    check.detail += " Identifier: " + identifier + ".";
                check.detail += team.isEmpty() ? " No developer team identifier (possibly ad hoc)."
                                               : " Team: " + team + ".";
                CFRelease(signing);
            }
        } else if (status == errSecCSUnsigned) {
            check.status = "Unsigned";
            check.level = "info";
            check.detail = "The selected executable has no code signature. " + osError(status);
        } else {
            check.status = "Validation failed";
            check.detail = osError(status) + " macOS may deny access to protected code.";
        }
        CFRelease(code);
        return check;
    }
}

QVector<Connection> Collector::connections(int pid, QString* error) {
    QVector<Connection> result;
    if (error)
        error->clear();
    if (pid <= 0) {
        if (error)
            *error = "Select a running process to inspect its TCP/UDP sockets.";
        return result;
    }
    errno = 0;
    const int bytes = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
    if (bytes < 0 || (bytes == 0 && errno != 0)) {
        if (error)
            *error = "macOS denied socket inspection or the process exited. No additional permissions are "
                     "requested.";
        return result;
    }
    if (bytes > 16 * 1024 * 1024) {
        if (error)
            *error = "This process has too many descriptors to inspect safely.";
        return result;
    }
    std::vector<proc_fdinfo> descriptors(static_cast<size_t>(bytes) / sizeof(proc_fdinfo) + 16);
    errno = 0;
    const int filled = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, descriptors.data(),
                                    static_cast<int>(descriptors.size() * sizeof(proc_fdinfo)));
    if (filled < 0 || (filled == 0 && errno != 0)) {
        if (error)
            *error = "The process exited or access to its descriptors was denied.";
        return result;
    }
    const QString name = QFileInfo(processPath(pid)).fileName();
    int denied = 0;
    const int count =
        std::min(filled / static_cast<int>(sizeof(proc_fdinfo)), static_cast<int>(descriptors.size()));
    for (int index = 0; index < count; ++index) {
        const auto& descriptor = descriptors[static_cast<size_t>(index)];
        if (descriptor.proc_fdtype != PROX_FDTYPE_SOCKET)
            continue;
        socket_fdinfo socket{};
        if (proc_pidfdinfo(pid, descriptor.proc_fd, PROC_PIDFDSOCKETINFO, &socket, sizeof(socket)) !=
            sizeof(socket)) {
            ++denied;
            continue;
        }
        const auto& info = socket.psi;
        if (info.soi_family != AF_INET && info.soi_family != AF_INET6)
            continue;
        Connection connection;
        connection.pid = pid;
        connection.process = name;
        if (info.soi_kind == SOCKINFO_TCP) {
            const auto& tcp = info.soi_proto.pri_tcp;
            connection.protocol = info.soi_family == AF_INET6 ? "TCP6" : "TCP4";
            connection.localAddress = socketEndpoint(tcp.tcpsi_ini, false);
            connection.remoteAddress = socketEndpoint(tcp.tcpsi_ini, true);
            connection.state = tcpState(tcp.tcpsi_state);
        } else if (info.soi_kind == SOCKINFO_IN && info.soi_protocol == IPPROTO_UDP) {
            connection.protocol = info.soi_family == AF_INET6 ? "UDP6" : "UDP4";
            connection.localAddress = socketEndpoint(info.soi_proto.pri_in, false);
            connection.remoteAddress = socketEndpoint(info.soi_proto.pri_in, true);
            connection.state = "Datagram";
        } else
            continue;
        result.append(std::move(connection));
    }
    if (error && denied)
        *error =
            QString("%1 sockets closed during inspection or could not be read. The list may be incomplete.")
                .arg(denied);
    return result;
}
} // namespace sentinel
