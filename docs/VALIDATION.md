# Validation record

Development checks performed on 2026-11-16 on an Apple Silicon Mac using Apple's command-line SDK and Clang. This record distinguishes observed results from release checks that still require another environment.

| Check | Observed result |
| --- | --- |
| GoogleTest mathematical and SQLite tests | 12/12 passed |
| CPU workload sanity check | A deliberately busy single CPU thread measured approximately 99.08% process CPU, consistent with 100% per fully used core |
| Collector overhead spot check | A live collection completed in approximately 2.58 ms on the development machine; this is a spot measurement, not a benchmark guarantee |
| Network collection | Actual physical-interface rate observed around 11.7 KiB/s; routing-message parser and 64-bit counters exercised |
| GPU hardware | Metal reported Apple M5 Pro; no GPU utilization value was invented |
| Process sockets | Permitted live TCP/UDP endpoint inspection returned valid endpoints |
| Code signature inspection | Security.framework recognized a valid signature; trust/notarization are explicitly separate |
| Qt runtime selection | Official Qt 6.8.3 framework metadata inspected; minimum macOS 12.0 allows this application's 14.0 target |
| Rejected dependency alternative | Local Homebrew QtCore required 14.4, and a local QtSvg bottle required macOS 27.0; these were not suitable for a macOS 14 package |
| Bundle verifier negative check | The unbundled app was correctly rejected for unresolved Qt dependencies and an absolute workspace Qt runtime path |
| Packaging shell / verifier syntax | Shell and Python syntax checks passed |
| ARM64 Release application | Built successfully with official Qt 6.8.3; macOS 14.0 deployment target |
| Full native smoke | Freshly extracted standalone package collected 69 real samples, visited all ten pages, and reported no history storage errors |
| Process UI interactions | Own-PID search and numeric resident-memory sorting passed; a no-match filtering assertion is included in the final smoke hook |
| Visual review | All ten pages captured and reviewed; compact 1000 × 700 and normal 1360 × 900 content layouts checked |
| Bundle dependencies | All 13 deployed Mach-O files verified to include ARM64, have minimum macOS versions no newer than 14.0, and resolve non-system dependencies within the app |
| Signature | Deep, strict ad-hoc signature verification passed; no Developer ID identity or notarization is claimed |
| Archive | Approximately 21 MB ZIP, extracted and launched successfully; SHA-256 checksum supplied |
| CLI failure handling | Nonfinite observation duration returns exit 2; screenshot export to a file returns exit 1 and reports capture failure |

The public repository includes four aggregate showcase screenshots in `docs/screenshots/`: Dashboard, CPU, Memory, and GPU. All ten pages were captured and reviewed locally. Additional QA screenshots and the JSON smoke report remain local and are excluded from publication because process, network, system, settings, and security views can reveal personal machine details. Self CPU readings in the local capture report include page switching, table sorting, and Retina image exports; they are not idle overhead benchmarks. The collector spot check above measures the collection routine separately.

The following are not established by development-machine checks: launch on a clean macOS 14.0 Mac, long-duration overhead/battery impact, all VPN/interface configurations, every APFS/network-mount layout, all protected-process permission combinations, and Apple's notarization acceptance. The first public [GitHub Actions run](https://github.com/Unknown-George/Sentinel/actions/runs/43935487640) passed the ARM64 build, all unit tests, live smoke check, bundle verification, packaging, and artifact upload on 2026-11-16. This provides an independent macOS 15 build check and does not establish launch on a clean macOS 14.0 installation. No Developer ID identity or notarization credentials were supplied.
