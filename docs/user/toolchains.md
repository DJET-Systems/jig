# Toolchains

## Choosing before you create

Choosing a standard narrows the compilers. jig shows only versions that accepted your standard when the index was built, because the index builder runs each compiler with `-std=<value> -fsyntax-only` and records the result.

```
$ jig toolchain available --lang c++ --std c++23
vendor  version  hermeticity  accepts
llvm    20.1.8   h1           c++14 c++17 c++20 c++23 c++26
llvm    19.1.7   h1           c++14 c++17 c++20 c++23 c++26
llvm    18.1.8   h1           c++14 c++17 c++20 c++23 c++26
```

(Illustrative output.)

"Accepts" means the compiler recognises the standard flag. It does not promise every library feature of that standard. `jig doctor --features` prints the feature-test macros measured for your compiler.

## Version specifications

| You write | Means |
|---|---|
| `clang@20` | Any 20.x.y, resolved to the highest |
| `clang@20.1` | Any 20.1.y |
| `clang@20.1.8` | Exactly this |
| `clang@^19` | `>=19.0.0, <20.0.0` |
| `clang@~20.1` | `>=20.1.0, <20.2.0` |
| `clang@>=18,<21` | A range |
| `clang@latest` | The highest stable version |

Prereleases are never chosen unless you name one or pass `--prerelease`.

## Vendors

| Vendor | Hermeticity | What it does |
|---|---|---|
| `llvm` | H1 | Downloads the official LLVM release for your host through the signed index. Provides `clang`, `clang++`, `clang-format`, `clang-tidy`, `clangd`, `lld`, `llvm-ar`. |
| `system` | H0 | Uses a compiler that is already on your machine (GCC, Clang, Apple Clang, MSVC). Nothing is downloaded or pinned. |
| `zig` | H3 | *(v0.3)* |
| `gcc` | | *(v0.4, after a provenance review)* |

CMake and Ninja are pinned the same way, under `[toolchain.tools]`.

## Commands

```
$ jig toolchain list                   installed in the store
$ jig toolchain available              installable (needs the index)
$ jig toolchain install clang@20
$ jig toolchain use clang@19           change this project's compiler
$ jig toolchain pin                    write the exact version into jig.toml
$ jig toolchain which c++              real path of a pinned tool
$ jig toolchain remove clang@18        refuses if a lock needs it
$ jig toolchain update                 refresh the index
```

## Offline use

Everything in the store works offline. Use `--offline` to make that a rule, and `--frozen` in CI to also require an up-to-date lock.

To work on a machine with no internet, copy the index folder and the archives to a directory, set `JIG_MIRROR` to it, and run jig as usual. The signature is still verified.

## When a version is missing

`DJ2101` means the version you asked for is not in the index for your host. `DJ2102` means it is, but it does not accept your standard. Both list the nearest alternatives.
