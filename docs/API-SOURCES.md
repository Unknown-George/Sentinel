# Primary API references

The installed macOS SDK headers and system manual pages are the immediate contract used by the Objective-C++ collector. The following upstream references explain those interfaces and the deployment constraints.

| Area | Primary reference |
| --- | --- |
| VM accounting | [Apple's XNU `vm_statistics.h`](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/vm_statistics.h) and the SDK `<mach/vm_statistics.h>` |
| Mach processor counters | [Apple's XNU `processor_info.h`](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/processor_info.h) and SDK `<mach/processor_info.h>` |
| Process and socket APIs | [Apple's libproc implementation](https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/libproc/libproc.c), SDK `<libproc.h>` / `<sys/proc_info.h>`, `man proc_pidinfo` |
| Memory pressure | [Dispatch memory pressure source](https://developer.apple.com/documentation/dispatch/dispatch_source_type_memorypressure) |
| Metal working set | [MTLDevice recommendedMaxWorkingSetSize](https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize) |
| Unified memory | [MTLDevice hasUnifiedMemory](https://developer.apple.com/documentation/metal/mtldevice/hasunifiedmemory) |
| Code signing | [Security Code Signing Services](https://developer.apple.com/documentation/security/code-signing-services), SDK `<Security/SecStaticCode.h>` |
| Interface enumeration | SDK `<ifaddrs.h>`, `<net/if.h>`, `<net/route.h>`, `man getifaddrs`, `man sysctl` |
| Filesystem capacity | SDK `<sys/mount.h>`, `man getmntinfo`, `man statfs` |
| FileVault and Gatekeeper | macOS `man fdesetup`, `man spctl`; firewall tool's own `--help` |
| SQLite | [SQLite C interface](https://sqlite.org/cintro.html) and [WAL](https://sqlite.org/wal.html) |
| Qt deployment | [Qt for macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html) |
| Qt platform minimums | [Qt for macOS](https://doc.qt.io/qt-6/macos.html) |
| Qt distribution obligations | [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations) and [module licensing](https://doc.qt.io/qt-6/licensing.html) |
| CI runner availability | [GitHub-hosted runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) |

Source links were checked during development on 2026-11-16. SDK headers are used as published interfaces; linking to Apple's open-source definitions does not authorize private or undocumented interfaces. Sentinel intentionally avoids private GPU/SMC metrics.
