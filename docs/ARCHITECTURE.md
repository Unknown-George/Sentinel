# Architecture

Sentinel is a single-process, unprivileged desktop application. The UI runs on Qt's main thread. System collection, history queries, and security inspection run on a dedicated Qt worker thread so slower process enumeration or bounded security subprocesses do not block interaction.

```text
macOS public APIs ── Collector ── Snapshot ── MonitorWorker
                                              │     │
                                      SQLite history │ queued signals
                                                    ▼
                                                MainWindow
                                              models / charts
```

## Ownership

`core/Types.h` defines value objects shared by the layers. Optional values distinguish a missing CPU/rate baseline from an observed zero. Snapshot timestamps use UTC wall-clock time for display/history; rate calculations use monotonic elapsed time.

`platform/Collector.mm` owns native API state and successive counter baselines. It is constructed, used, and destroyed on the collection thread. The collector does not depend on UI objects. It handles resources with scoped ownership, including Mach processor arrays, Core Foundation objects, and interface lists. Process CPU baselines include process identity so PID reuse cannot turn an earlier process's cumulative time into the next process's measurement.

`core/MonitorService` is the main-thread facade. Requests cross to the worker via queued calls; results are copied through queued Qt signals. A worker-owned timer collects at the configured interval. SQLite persistence is limited to one aggregate record per five seconds, with periodic retention. The worker also performs security checks and selected-process inspection on demand. Shutdown stops the worker event loop and joins its thread before releasing the service.

`core/HistoryStore` owns one SQLite connection used only on the worker thread. It enables WAL, uses prepared statements, stores missing floating-point measurements as SQL NULL, and averages into time buckets inside SQLite so a long history query does not materialize every retained sample. The schema is versioned with `user_version`. The store reports errors for display without replacing real data with generated values.

`ui/` owns navigation, table presentation, bounded graph buffers, user preferences, and the original vector mascot. Native macOS window controls remain in the platform titlebar. Reduced-motion preferences disable decorative animation. Charts label time and units; historical bucket values represent averages, not reconstructed individual measurements.

## Extending a metric

Add a value and validity representation to the shared types, implement it in the platform collector, document the API/formula and permissions, then add the UI. Only persist aggregate values that have an explicit retention and privacy purpose. Add a test for any nontrivial calculation or missing/reset behavior. Do not turn an API failure into a zero or derive hardware counters from undocumented IOKit properties.

## Security model

The application observes the current user's system through normal permissions. It does not grant additional access by running as root, request Full Disk Access, alter security preferences, terminate processes, or install helpers. Read-only system tools are launched by absolute path with argument arrays and timeouts. Selected executable signing inspection uses Security.framework rather than executing the inspected binary.

Process and socket lists are inherently racy: a process may exit or close a descriptor after enumeration. Permission errors and unsupported information must remain visible as incomplete observations. UI security status is a set of independent checks, not an overall security score.
