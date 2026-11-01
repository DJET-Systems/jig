# Platform sheet

## Support tiers

| Tier | Meaning |
|---|---|
| 1 | Built, tested and released on every change. Blocking in CI. |
| 2 | Built and released. Tested on a schedule. Not blocking. |
| 3 | Community effort. No promises. |

| Host | Triple | Tier (v0.1) |
|---|---|---|
| Linux x86_64 | `x86_64-unknown-linux-gnu` | 1 |
| macOS arm64 | `aarch64-apple-darwin` | 1 |
| Windows x86_64 | `x86_64-pc-windows-msvc` | 1 |
| Linux aarch64 | `aarch64-unknown-linux-gnu` | 2 |
| macOS x86_64 | `x86_64-apple-darwin` | 2 |
| Windows arm64 | `aarch64-pc-windows-msvc` | 3 |
| FreeBSD, others | | 3 |

The tier list is a plan. The index builder decides which toolchains exist per host; `jig toolchain available` always reports the truth for the machine it runs on.

## What each platform needs from the machine (v0.1, hermeticity H1)

| Platform | Pinned by jig | Still from the machine | Prerequisite message |
|---|---|---|---|
| Linux | compiler, CMake, Ninja | libc (glibc or musl as the toolchain entry requires), system headers, C++ runtime unless the entry says otherwise | `DJ3203` names the required glibc version |
| macOS | compiler, CMake, Ninja | macOS SDK and system libraries | `DJ3202`: `xcode-select --install` |
| Windows | compiler (clang), CMake, Ninja | MSVC runtime, Windows SDK from Visual Studio Build Tools | `DJ3201`: install "Desktop development with C++" |

jig does not bundle the macOS SDK or the Windows SDK in v0.1 (decision D-009).

## Shim kind per platform

| Platform | Default shim | Status |
|---|---|---|
| Linux | `link` | To be confirmed by T-005-03 |
| macOS | `link` | To be confirmed by T-005-03 |
| Windows | `launcher` | Symlinks need privileges; launcher is the default |

The "to be confirmed" cells are filled in with the result of the real-toolchain test and committed here.

## Default directories

| Purpose | Linux | macOS | Windows |
|---|---|---|---|
| `JIG_HOME` | `$XDG_DATA_HOME/jig` or `~/.local/share/jig` | `~/Library/Application Support/jig` | `%LOCALAPPDATA%\jig` |
| Config | `$XDG_CONFIG_HOME/jig` or `~/.config/jig` | `~/Library/Application Support/jig` | `%APPDATA%\jig` |
| Installed binary | `~/.local/bin/jig` | `~/.local/bin/jig` | `%LOCALAPPDATA%\Programs\jig\jig.exe` |

## Shell support

| Shell | `jig env` | `activate` | `jig shell` |
|---|---|---|---|
| bash, zsh | `sh` | `activate` | yes |
| fish | `fish` | `activate.fish` | yes |
| PowerShell | `pwsh` | `activate.ps1` | yes |
| cmd | `cmd` | `activate.bat` | yes |

## Release artifact static-linking summary

| Platform | Link mode |
|---|---|
| Linux | fully static (musl), glibc fallback build |
| macOS | system `libc++`, everything else static |
| Windows | static CRT (`/MT`), everything static |
