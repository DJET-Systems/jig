# jig documentation

**jig, the native C/C++ toolchain.**
Pin the compiler. Pick the standard. Build in an environment that belongs to the project.

> **Status.** jig 0.1 is the first release of the design in this repository. Output shown in these pages is representative; versions, sizes and hashes are examples. Pages marked *(v0.2)* describe planned behaviour.

## Where to start

| If you want to | Read |
|---|---|
| Install jig and build something in five minutes | [Getting started](getting-started.md) |
| Understand the ideas before the commands | [Concepts](concepts.md) |
| Choose a compiler version for a language standard | [Toolchains](toolchains.md) |
| Write or edit `jig.toml` | [Projects and the manifest](projects.md) |
| Understand `.jig/` and activation | [Environments](environments.md) |
| Build, run, test, format | [Building](building.md) |
| Know exactly how much jig pins | [Hermeticity](hermeticity.md) |
| Compare with other tools | [Comparison](comparison.md) |
| Fix a problem | [Troubleshooting](troubleshooting.md) and [Error codes](../reference/errors.md) |
| Look something up | [CLI reference](../reference/cli.md) · [Manifest reference](../reference/manifest.md) · [Cheat sheet](../reference/cheatsheet.md) |
| Quick answers | [FAQ](faq.md) |

## What jig is

A single program that creates a C or C++ project after you choose the language, the standard and the compiler **version**, downloads that compiler (and CMake and Ninja) from a signed index, and builds the project inside an isolated environment that lives in the project folder. It narrates what it does and why.

## What jig is not

- It is not a new build system. It generates CMake and runs Ninja.
- It is not a package manager. Dependencies arrive in v0.2 by handing off to vcpkg, git and local paths.
- It does not hide the platform SDKs. On macOS and Windows you still need Apple's command line tools or Visual Studio Build Tools; jig checks and tells you.

## Conventions in these pages

- `$` starts a command you type. Lines without it are output.
- `▸` marks a step jig is taking, `◆` a decision it made, `│` the reasons, `✓` success, `!` warning, `✗` error.
- Version numbers are examples. Run `jig toolchain available` to see what exists today.
