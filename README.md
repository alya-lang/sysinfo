# sysinfo

[![CI](https://github.com/alya-lang/sysinfo/actions/workflows/ci.yml/badge.svg)](https://github.com/alya-lang/sysinfo/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/alya-lang/sysinfo?color=blue&label=License)](LICENSE)
[![Alya](https://img.shields.io/badge/dynamic/toml?url=https%3A%2F%2Fraw.githubusercontent.com%2Falya-lang%2Fsysinfo%2Fmain%2Falya.toml&query=%24.package.alya-version&label=Alya&color=orange&prefix=%3E%3D)](https://github.com/alya-lang/alya)
[![Package Version](https://img.shields.io/badge/dynamic/toml?url=https%3A%2F%2Fraw.githubusercontent.com%2Falya-lang%2Fsysinfo%2Fmain%2Falya.toml&query=%24.package.version&label=Version&color=brightgreen)](alya.toml)

Cross-platform system information: locale, language, timezone and OS details for Alya

---

## 🌟 Features

- 🖥️ **OS Identification**: Canonical platform id, human-readable version (Windows `RtlGetVersion`, macOS `kern.osproductversion`, Linux `/etc/os-release`), CPU arch
- ⚡ **CPU Reporting**: Logical core count and model string per OS (Windows `GetSystemInfo`, macOS `sysctl`, Linux `/proc/cpuinfo`)
- 🧠 **Memory Reporting**: Total/available RAM with MB and usage-percent helpers (Windows `GlobalMemoryStatusEx`, macOS `host_statistics64`, Linux `/proc/meminfo`)
- 💾 **Disk Usage**: Total/free bytes per path (Windows `GetDiskFreeSpaceEx`, POSIX `statvfs`)
- 🏠 **Host Identity**: Hostname and username via native API with env fallback
- 🕐 **Timezone & Uptime**: System timezone name + UTC offset, seconds-since-boot
- 🌍 **Locale Detection**: Native locale API with `LANGUAGE`/`LC_*`/`LANG` fallback, BCP-47 normalization, fallback chains
- 🧩 **Modular Architecture**: Layered multi-module design with a clean public facade (`src/lib.alya`) and canonical models (`src/types.alya`)
- 🔒 **Public/Private Visibility (`pub`)**: Fine-grained export control keeping internals encapsulated
- 🧪 **Enterprise Test & Benchmark Suite**: Real-hardware assertions (`std/test`) and micro-benchmarking

---

## 📁 Project Architecture

```
sysinfo/
├── .alyalint               # Linter configuration (rules, exclusions, severity overrides)
├── .editorconfig           # Uniform formatting rules across IDEs and editors
├── .gitignore              # Ecosystem standard ignore filters
├── .vscode/                # VS Code workspace settings, DAP launch configurations & tasks
├── alya.toml               # Package manifest with dependencies and optional [build]
├── c/                      # Native C sources for zero-dependency FFI packages
│   ├── sysinfo.h           # Shared native declarations
│   ├── sysinfo.c           # Common fallback engine
│   ├── win32_locale.c      # Windows locale (GetUserDefaultLocaleName)
│   ├── win32_sysinfo.c     # Windows OS/CPU/RAM/disk/TZ/uptime
│   ├── cocoa_locale.c      # macOS locale (CFLocale)
│   ├── cocoa_sysinfo.c     # macOS sysctl/VM/statvfs/timezone
│   ├── linux_locale.c      # Linux locale (setlocale/env)
│   └── linux_sysinfo.c     # Linux /proc/statvfs/timezone
├── src/
│   ├── lib.alya            # Public API facade (system_info, summary, locale)
│   ├── types.alya          # Data models (OsInfo, CpuInfo, MemInfo, DiskUsage, ...)
│   ├── ffi.alya            # Native extern "C" declarations
│   ├── locale.alya         # Locale detection, normalization, fallback chains
│   ├── os_info.alya        # OS name/version/arch
│   ├── cpu.alya            # CPU cores/model
│   ├── mem.alya            # Memory totals
│   ├── disk.alya           # Disk usage per path
│   ├── host.alya           # Hostname/username
│   ├── timezone.alya       # Timezone name/offset
│   ├── uptime.alya         # Uptime counters
│   └── core/               # Subdirectory module hierarchy
│       └── formatter.alya  # format_bytes, format_duration, format_system_summary
├── examples/
│   └── demo.alya           # Runnable walkthrough of all package capabilities
├── tests/
│   └── test_basic.alya     # Automated test suite (real-hardware assertions)
└── benches/
    └── bench_basic.alya    # Micro-benchmarks measuring performance and throughput
```

> [!NOTE]
> **Visibility & Modularity:** Symbols annotated with `pub` (`pub function`, `pub struct`, `pub enum`, `pub interface`) are exported to external consumers and re-exporting modules. Symbols without `pub` remain strictly internal to their declaring module, preventing symbol collisions and implementation leakage.

---

## 📦 Installation

Add `sysinfo` to the `[dependencies]` section in your `alya.toml`:

```toml
[dependencies]
sysinfo = { git = "https://github.com/alya-lang/sysinfo", branch = "main" }
```

Or install it directly using the Alya package CLI:

```bash
alya add sysinfo --git https://github.com/alya-lang/sysinfo --branch main
alya install
```

---

## 🚀 Quick Start

```alya
import "sysinfo" as pkg

function main()
    # 1. Full snapshot in one call
    let info = pkg::system_info()
    say f"OS:      {info.os.version}"
    say f"CPU:     {info.cpu.model} x{info.cpu.cores}"
    say f"Memory:  {pkg::format_mem(info.mem)}"
    say f"Locale:  {info.locale}"

    # 2. One-line summary
    say pkg::summary()
end

main()
```

---

## 📖 API Reference

| Symbol | Visibility | Description |
|---|---|---|
| `system_info()` | `pub function` | Collects a full `SystemInfo` snapshot (OS, CPU, RAM, disk, host, TZ, uptime, locale). |
| `summary()` | `pub function` | Returns a one-line human-readable system summary. |
| `os_name()` | `pub function` | Canonical OS id (`"windows"`, `"linux"`, `"macos"`). |
| `os_version()` | `pub function` | Human-readable OS version with OS-id fallback. |
| `os_arch()` | `pub function` | Canonical CPU arch id (`"x64"`, `"x86"`, `"arm64"`). |
| `os_info()` | `pub function` | Collects an `OsInfo` record. |
| `cpu_cores()` | `pub function` | Logical processor count (>= 1). |
| `cpu_model()` | `pub function` | Human-readable CPU model ("" when unavailable). |
| `cpu_info()` | `pub function` | Collects a `CpuInfo` record. |
| `mem_total_bytes()` | `pub function` | Total physical RAM in bytes (-1 when unavailable). |
| `mem_avail_bytes()` | `pub function` | Available physical RAM in bytes (-1 when unavailable). |
| `mem_info()` | `pub function` | Collects a `MemInfo` record. |
| `disk_total_bytes(path)` | `pub function` | Filesystem total bytes for path (-1 when unavailable). |
| `disk_free_bytes(path)` | `pub function` | Filesystem free bytes for path (-1 when unavailable). |
| `disk_usage(path)` | `pub function` | Collects a `DiskUsage` record (defaults to `disk_default_path()`). |
| `host_name()` | `pub function` | Machine hostname via native API with env fallback. |
| `host_user()` | `pub function` | Current user name via env. |
| `host_info()` | `pub function` | Collects a `HostInfo` record. |
| `tz_name()` | `pub function` | System timezone name with `TZ` fallback. |
| `utc_offset_min()` | `pub function` | UTC offset in minutes (UTC+3 -> 180). |
| `tz_info()` | `pub function` | Collects a `TzInfo` record. |
| `uptime_sec()` | `pub function` | Seconds since boot (0 when unavailable). |
| `system_lang(default)` | `pub function` | Best system locale tag (`"tr-TR"`), `"en"` fallback. |
| `system_langs()` | `pub function` | Locale fallback chain (`["tr-TR", "tr", "en"]`). |
| `normalize_locale(raw)` | `pub function` | Normalizes `"tr_TR.UTF-8"` to `"tr-TR"`. |
| `locale_info()` | `pub function` | Collects a `LocaleInfo` record for the system locale. |
| `format_bytes(n)` | `pub function` | Formats bytes as `"15 GB"`, `"512 MB"`, `"unknown"` when negative. |
| `format_duration(sec)` | `pub function` | Formats seconds as `"3d 4h"`, `"5h 12m"`, `"45s"`. |
| `format_utc_offset(min)` | `pub function` | Formats minutes as `"UTC+3"`, `"UTC"`. |
| `format_mem(info)` | `pub function` | One-line RAM overview (`"RAM 13289/16127 MB (82%)"`). |
| `format_disk(usage)` | `pub function` | One-line disk overview (`"C:/ 188/199 GB (94%)"`). |
| `format_system_summary(info)` | `pub function` | Full multi-field snapshot line. |
| `OsInfo` | `pub struct` | OS model (`name`, `version`, `arch`). |
| `CpuInfo` | `pub struct` | CPU model (`arch`, `cores`, `model`). |
| `MemInfo` | `pub struct` | Memory model (`total_bytes`, `avail_bytes`) + `total_mb()`, `avail_mb()`, `used_percent()`, `is_known()`. |
| `DiskUsage` | `pub struct` | Disk model (`path`, `total_bytes`, `free_bytes`) + `used_bytes()`, `used_percent()`, `is_known()`. |
| `HostInfo` | `pub struct` | Host model (`hostname`, `username`). |
| `TzInfo` | `pub struct` | Timezone model (`name`, `offset_min`). |
| `LocaleInfo` | `pub struct` | Locale model (`bcp47`, `lang`, `region`). |
| `SystemInfo` | `pub struct` | Aggregated snapshot (`os`, `cpu`, `mem`, `disk`, `host`, `tz`, `uptime_sec`, `locale`). |

> [!TIP]
> **Internal Helpers & Documentation:** Public symbols are documented with `##` Markdown docstrings, enabling automatic API documentation generation via `alya doc`. Private helpers remain encapsulated without `pub`.

---

## 🧪 Running Tests & Benchmarks

Run the automated test suite using `alya test`:

```bash
alya test
```

Generate static API documentation:

```bash
alya doc . -o docs --markdown
```

Run the benchmark suite:

```bash
alya run benches/bench_basic.alya
```

Run the example demo:

```bash
alya run examples/demo.alya
```

Check code formatting:

```bash
alya fmt . --check
```

Run static code linter:

```bash
alya lint . --check
```

---

### 💻 Developer Tooling & VS Code Integration

This package comes preconfigured with recommended workspace settings and tasks for **Visual Studio Code**:
- **LSP & Formatting**: Auto-formatting on save and real-time Language Server diagnostics via `alya-lang.vscode-alya`.
- **DAP Debugging**: Launch configurations in `.vscode/launch.json` ready for interactive step-debugging via `F5`.
- **Predefined Tasks**: Press `Ctrl+Shift+B` or run tasks (`Test`, `Lint`, `Format`, `Build Docs`) directly from the Command Palette.

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository and clone it locally
2. Install dependencies:
   ```bash
   alya install
   ```
3. Create your feature branch (`git checkout -b feature/my-feature`)
4. Verify tests and formatting before opening a PR:
   ```bash
   alya test
   ```
5. Commit your changes (`git commit -m "feat: add feature"`) and open a Pull Request

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.