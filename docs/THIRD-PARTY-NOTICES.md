# Third-party notices

Sentinel's application code, vector mascot, and original visual assets are released under the MIT license in the repository root. Dependencies retain their own licenses.

| Component | Role | License / upstream |
| --- | --- | --- |
| Qt Core, GUI, Widgets, SVG and deployed plugins | Desktop application and rendering | Qt's open-source distribution offers LGPL-3.0/GPL alternatives for these modules; see [Qt licensing](https://doc.qt.io/qt-6/licensing.html), [Qt source](https://code.qt.io/cgit/qt/) and the component licenses supplied with the exact Qt release. |
| SQLite | Local aggregate history | [Public domain](https://sqlite.org/copyright.html); this build uses the system SQLite library. |
| GoogleTest | Build-time unit tests only | [BSD-3-Clause](https://github.com/google/googletest/blob/main/LICENSE); not included in the application. |
| macOS frameworks and system libraries | System integration | Provided by Apple as part of macOS; not copied into the application. |

The application links dynamically to Qt. The packaging script includes the LGPL-3.0 and GPL-3.0 license texts, these notices, and Qt's component inventory when available. Qt includes additional third-party software; distributors must retain the copyright and license notices for the exact modules and plugins deployed. A release must include the corresponding Qt source or an appropriate source offer, including any modifications. Publishing Sentinel's MIT sources alone does not satisfy all Qt distribution obligations.

The supplied application's unmodified upstream Qt libraries are version **6.8.3**. Their corresponding source archives, including third-party source and licenses, are available from Qt:

- [Qt Base 6.8.3 source](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz)
- [Qt SVG 6.8.3 source](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtsvg-everywhere-src-6.8.3.tar.xz)

Qt's SDK-provided SPDX inventories retain the release's component and copyright information in the packaged `licenses/qt-sbom/` directory. The package script copies the inventory from either the official SDK's `sbom/` or the Homebrew installation's `share/qt/sbom/`. When distributing copies, preserve access to corresponding source under the license terms; upstream links are not a substitute for the distributor's obligations if they stop working. No modifications to Qt source are required by this project; CMake compatibility adjustments apply to Sentinel's compiler/link options.

Recipients may rebuild Sentinel against compatible modified Qt libraries. Build instructions are in README.md. The application uses ordinary framework loading and does not intentionally restrict library replacement. Replacing frameworks invalidates an existing code signature; a local rebuild or ad-hoc re-signing restores executability. Do not add license terms that prohibit reverse engineering for debugging modifications to LGPL libraries. See the [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations) and the included license texts for the governing terms.

The package script's ad-hoc signature is for local execution, not proof of publisher identity or notarization. Developer ID signing and notarization are available only when the release operator provides the required Apple credentials.
