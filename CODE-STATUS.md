# jig: code status

Pre-release, first code drop (0.1.0). This file says what the code does today, what it does not, and how to review it.

## Build and test

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/jig --help
```

Needs: a C++23 compiler (GCC 13+ or Clang 17+), CMake 3.24+, Ninja. All third-party code is vendored in `third_party/` (CLI11 2.4.2, toml++ 3.4.0, tl::expected 1.1.0).

Options: `-DJIG_WARNINGS_AS_ERRORS=ON`, `-DJIG_BUILD_TESTS=OFF`, `-DJIG_STATIC=ON` (untested). Presets: `cmake --preset dev`.

## Tested environment

| Item | Value |
|---|---|
| OS | Ubuntu 24.04, x86_64 |
| Compilers | GCC 13.3.0, Clang 18.1 (both with `-Werror`) |
| CMake / Ninja | 3.28.3 / 1.11.1 |
| Result | unit tests (15) and e2e script (about 38 checks) pass |

Not tested: macOS, Windows, other Linux distributions, ARM, the static build, the GitHub Actions workflow.

## What works today

- `jig init`: interactive wizard (numbered prompts) or flags; C and C++; exe, static-lib, shared-lib, header-only; language standard and compiler chosen before files are written; refuses non-empty directories; optional `git init`.
- `jig.toml` manifest with strict validation: unknown keys are errors with did-you-mean (DJ1001 to DJ1008).
- `jig.lock`: manifest digest, compiler identity and sha256, cmake and ninja versions. `jig lock`, `--locked`, `--frozen`, `--check`.
- Per-project environment in `.jig/env`: shims for cc/c++/cmake/ninja, activate scripts (`jig env --format sh|fish|json`), `jig shell`, `jig exec -- cmd`. Variables such as CFLAGS, CXXFLAGS, CC, CXX are dropped by default (`[env] inherit`).
- `jig toolchain list | available | which | use | pin`.
- `jig build [--release|--profile]`, `run`, `test`, `fmt [--check]`, `clean`, `doctor`, `cache`, `version`.
- Profiles: debug, release, user-defined, inheritance, sanitizers, LTO, defines.
- Backends: generated CMake from `jig.toml`, or your own `CMakeLists.txt` (`[build] backend = "cmake"`).
- `compile_commands.json` symlink, tests as one executable per file through CTest.
- Verbosity: `-q`, `-qq`, `-v` up to `-vvvv`, `--terse`, `--explain`, `--json` (event stream), `NO_COLOR`.

## What is not implemented

| Area | State |
|---|---|
| `llvm` provider (download, signed index, store, hermeticity H1) | designed, not built. `toolchain install/remove/update` return DJ9001 |
| Only `system` provider | compilers found on PATH, hermeticity H0 (path and sha256 recorded, not downloaded) |
| Dependencies | planned for 0.2 |
| `[[target]]` multi-target | rejected with DJ9001 |
| Windows / MSVC | POSIX only (fork/exec, symlinks) |
| `completions`, `self update` | absent |
| `--json` | event stream only, not a stable API yet |
| Signature verification | not present. The built-in SHA-256 is for digests only |

## Layout

```
src/jig-core    errors, versions, sha256, glob, process, platform, manifest, lock, TOML editing
src/jig-ui      the only code that prints (events rendered as narrated, terse or JSON)
src/jig-env     toolchain detection and resolution, env sync, shims, project preparation
src/jig-build   CMake generation, build, run, test, fmt, clean, exec
src/jig-init    templates and the init wizard
src/jig         main.cpp (CLI11 wiring), commands, doctor
tests/          unit_tests.cpp, e2e.sh
```

Docs in `docs/blueprint/` describe the intended design; where they differ from the code, this file wins.

## Review checklist

1. `src/jig-core/error.hpp`: error model and `JIG_TRY` macros. Everything depends on them.
2. `src/jig-core/manifest.cpp`: validation and did-you-mean.
3. `src/jig-env/toolchain.cpp`: compiler detection, `std_flag` table (version thresholds), resolution.
4. `src/jig-env/env.cpp`: atomic env swap, shims, variable dropping.
5. `src/jig-core/process.cpp`: fork/exec, pipe handling, signals.
6. `src/jig-build/cmakegen.cpp`: generated CMake, profile flags.
7. `src/jig-init/init.cpp`: wizard and flag handling.

Known rough edges worth testing: unusual compiler wrappers (ccache, distcc), paths with spaces, read-only directories, Ctrl-C during a build, a compiler removed after locking.

## Name

"jig" has not been cleared for trademark or registry conflicts. Error codes keep the `DJ` prefix.
