# Metric definitions

Byte values are collected as bytes. The interface uses binary units (KiB, MiB, GiB) and bytes per second for throughput. A missing metric is rendered as unavailable or collecting rather than zero.

## CPU

For each processor, `host_processor_info(PROCESSOR_CPU_LOAD_INFO)` provides cumulative user, system, idle, and nice tick counters. Given two valid samples:

```text
busy  = Δuser + Δsystem + Δnice
total = busy + Δidle
CPU % = 100 × busy / total
```

Overall CPU uses the sum of tick differences across all observed logical cores. A zero interval, reset, wrap, or missing baseline yields no measurement. The interval is the actual elapsed time between collections, not an assumption about timer punctuality.

Process CPU uses `proc_taskinfo`'s cumulative user and system absolute time, converts it to nanoseconds with `mach_timebase_info`, and divides its difference by actual monotonic elapsed seconds. One CPU-second per elapsed second is 100%. Processes may use several cores, so their percentage is not bounded to 100. Baselines are discarded when process identity changes or counters decrease. Resident process memory is RSS, not the process's full virtual address space, and shared resident pages can appear in multiple processes.

## Memory

The collector reads physical RAM from `hw.memsize`, the host page size, and `host_statistics64(HOST_VM_INFO64)`. Page counts are multiplied by the host page size, which must not be assumed to be 4096 bytes on Apple Silicon.

| Label | Meaning |
| --- | --- |
| Total | Physical RAM reported by the OS |
| Wired | `wire_count × pageSize`; pages not normally pageable |
| Compressed | `compressor_page_count × pageSize`; physical pages occupied by the compressor, not the uncompressed logical size |
| Used (estimate) | `min(total, wired + compressed + max(internal_page_count − purgeable_count, 0) × pageSize)` |
| Available (estimate) | `total − used`; reclaimable headroom including cache/reclaimable memory, not immediately free pages |
| Active / inactive / free | Raw VM queue counts multiplied by page size; these categories are not additive with every other displayed metric |
| Purgeable | Purgeable page count multiplied by page size; deducted from internal pages for the used estimate |
| Swap | Used and total bytes from `vm.swapusage`; a separate disk-backed resource, shown unavailable if the query fails |
| Pressure | Latest observed `DISPATCH_SOURCE_TYPE_MEMORYPRESSURE` notification; Unknown before a notification |

The memory estimate is intended to distinguish reclaimable file caches from nonpurgeable application/wired/compressed use. It is not guaranteed to exactly reproduce Activity Monitor's private accounting. A high used-memory percentage alone is not evidence of memory pressure. The pressure indicator is event-based and can remain unknown during an entire quiet session. VM, swap, and uptime have independent validity flags; a failed query must not turn into an observed zero.

## Network

Interface byte counters are differenced against the same interface's previous counters and divided by monotonic elapsed seconds. A newly observed interface, a counter decrease, or an invalid elapsed interval yields no rate. Interface names and IP addresses are resolved through `getifaddrs`; byte counters use the public routing-interface information interface. The UI lists interfaces independently so their scope remains visible.

Aggregate throughput includes active (`IFF_UP` and `IFF_RUNNING`), non-loopback `en*` interfaces of type `IFT_ETHER`. Tunnel, bridge, and other virtual interfaces remain listed separately and are excluded from the total to avoid VPN double counting. This is a physical-link estimate, not an application-level count of payload bytes or an internet speed test; unusual hardware/interface naming can exclude a link. Interface changes and sleep/wake can produce collection gaps. Local/remote socket endpoints are inspected only for an explicitly selected process using libproc where permissions allow. Endpoints do not reveal encrypted payloads or prove that a connection is malicious.

## Storage

Volume capacity and availability come from cached mounted filesystem statistics (`getmntinfo` with `MNT_NOWAIT`). Local and network mounts with nonzero block counts are included; device filesystems are excluded. Used is total minus available. Availability is the filesystem's current reported available space; APFS sharing, purgeable data, quotas, snapshots, and user permissions can make Finder's presentation differ. Cached network-mount statistics can be stale. Mounted APFS volumes can share capacity, so their totals must not be summed into a physical-device total. The historical system-disk series prefers `/System/Volumes/Data` and falls back to `/`.

Capacity history records occupancy, not disk read/write throughput. Sentinel does not use undocumented storage-controller counters to invent an I/O graph.

## GPU

Metal provides the device name, unified-memory flag, and recommended maximum working-set size. The recommendation describes an allocation budget approximation; it is neither current GPU memory use nor a dedicated VRAM capacity. Total GPU utilization, power, temperature, clock speed, and historical GPU-load curves are unavailable through the stable public APIs used here.

## History

UTC timestamps accompany aggregate measurements. Samples are persisted no more frequently than every five seconds, independent of the faster live refresh setting. Long queries average existing values into bounded time buckets; they do not interpolate gaps or simulate missing measurements. CPU and network SQL NULL values remain missing unless a bucket contains other actual measurements. The point timestamp is the last observation in that bucket. Each point retains its averaging-window width so longer views connect adjacent historical buckets while leaving larger collection gaps visible. Missing memory/storage data has a zero total so a UI can treat it as unavailable rather than a valid percentage.

## Security checks

Firewall, FileVault, and Gatekeeper results are interpreted only from successful recognizable output of the macOS-provided read-only tools. An access denial, timeout, command failure, or unrecognized result is Unknown. Signing inspection checks the selected code's signature validity and available signing metadata. An ad-hoc or valid signature alone does not establish publisher identity, notarization, or benign behavior. These independent facts are never reduced to a security score.
