# jig 0.1.0

> **Draft.** Fill every `TODO` before publishing. Delete this box.

**Release date:** TODO
**Tag:** `v0.1.0`
**Index format:** v1
**Manifest / lock version:** 1 / 1

## In one paragraph

jig 0.1 is the first release of the native C/C++ toolchain from DJET Systems. You choose a language, a standard and a compiler version, and jig creates the project, downloads that compiler from a signed index, and builds inside an environment that belongs to the project. It narrates what it does and why, and it reports honestly how much of the build it pins.

## Highlights

- **Choose before you create.** `jig init` asks for the standard first and shows only compiler versions that accept it.
- **Pinned toolchains.** Official LLVM releases, CMake and Ninja, verified by signature and sha256, recorded in `jig.lock`.
- **A project environment.** `.jig/env` with shims, a CMake toolchain file and activation scripts for bash, zsh, fish, PowerShell and cmd.
- **Build, run, test, format.** Generated CMake, Ninja, CTest, pinned `clang-format`, `compile_commands.json`.
- **Narrated output.** Decisions with reasons by default; `--explain`, `-vv`, `--json`.
- **`jig doctor`.** Host and project checks, hermeticity level, bug-report bundle.
- **One static binary** on Linux, macOS and Windows.

## Supported platforms

| Platform | Tier |
|---|---|
| Linux x86_64, macOS arm64, Windows x86_64 | 1 |
| Linux aarch64, macOS x86_64 | 2 |

## Install

```
curl -fsSL https://djet.online/jig/install.sh | sh
irm https://djet.online/jig/install.ps1 | iex
```

Manual downloads and signatures: TODO link.

## Verify

| Artifact | sha256 |
|---|---|
| `jig-0.1.0-x86_64-unknown-linux-gnu.tar.xz` | TODO |
| `jig-0.1.0-aarch64-unknown-linux-gnu.tar.xz` | TODO |
| `jig-0.1.0-aarch64-apple-darwin.tar.xz` | TODO |
| `jig-0.1.0-x86_64-apple-darwin.tar.xz` | TODO |
| `jig-0.1.0-x86_64-pc-windows-msvc.zip` | TODO |

Release signing key (ed25519, public): TODO. `SHA256SUMS.sig` verifies `SHA256SUMS`. The SBOM is `jig-0.1.0.cdx.json`.

## What is not in 0.1

| Item | When |
|---|---|
| Dependencies (`jig add`, vcpkg, git, path) | 0.2 |
| jig building itself with jig | 0.2 |
| Zig `cc`, pinned C++ runtime (H2), hermetic builds (H3), cross-compilation | 0.3 |
| GCC provider, `jig check` (clang-tidy), package-manager distribution | 0.4 |

jig 0.1 reaches hermeticity H1 (pinned compiler). The C library, system headers and the platform SDK still come from your machine. macOS needs the Xcode command line tools and Windows needs Visual Studio Build Tools.

## Compatibility

- `jig.toml`: unknown keys are errors.
- `jig.lock`: version 1. Newer jig reads it; older jig refuses a newer one.
- Error codes `DJxxxx` are stable from this release.
- Event names and listed fields in `--json` are stable within 0.x.

## Known issues

TODO (list real ones found in the release candidate; do not leave empty without checking).

## Thanks

TODO

## Upgrading

First release. Nothing to upgrade.

## Links

[Documentation](https://djet.online/jig/docs) · [Changelog](../CHANGELOG.md) · [Report an issue](https://github.com/ansgrb/jig/issues) · [Security policy](../SECURITY.md)
