# Projects and the manifest

The full key list is in the [Manifest reference](../reference/manifest.md). This page explains the choices.

## Project kinds

| Kind | Produces | Use for |
|---|---|---|
| `exe` | An executable | Applications and tools |
| `static-lib` | A static library | Libraries you link in |
| `shared-lib` | A shared library | Plugins and shared components |
| `header-only` | Headers only | Small template libraries |

## Languages and standards

| Language | Standards |
|---|---|
| C | `c11`, `c17`, `c23` |
| C++ | `c++14`, `c++17`, `c++20`, `c++23`, `c++26` (experimental) |

`extensions = true` selects the compiler's GNU dialect (`-std=gnu++23`). The default is off, so the project builds as portable standard code.

## Default layout

```
src/            sources
include/        public headers (libraries) and project headers
tests/          one executable per file, run through CTest
jig.toml
jig.lock
```

Change it with `[build] sources`, `include` and `public-include`.

## Profiles

`debug` and `release` exist by default. Add your own:

```toml
[profile.asan]
inherits   = "debug"
sanitizers = ["address", "undefined"]

[profile.fast]
inherits = "release"
lto      = "full"
```

```
$ jig build --profile asan
$ jig test   --profile asan
```

Each profile has its own build directory, so switching does not rebuild everything.

## Warnings

`warnings = "strict"` turns on `-Wall -Wextra -Wpedantic` (or `/W4` for MSVC-style compilers). `off` disables warnings. `default` leaves the compiler default.

## Tests

Every file under `tests/` becomes one executable registered with CTest. A test passes when its `main` returns 0.

```cpp
#include <cstdlib>
int main() { return (1 + 1 == 2) ? EXIT_SUCCESS : EXIT_FAILURE; }
```

Test frameworks arrive with dependencies in v0.2.

## Several targets

```toml
[[target]]
name    = "core"
kind    = "static-lib"
sources = ["src/core/**/*.cpp"]

[[target]]
name         = "cli"
kind         = "exe"
sources      = ["src/cli/**/*.cpp"]
link-targets = ["core"]
```

With several executables, name the one to run: `jig run cli`.

## Adopting an existing CMake project

```
$ cd existing-project
$ jig init --adopt --lang c++ --std c++20 --toolchain clang@19
$ jig build
```

jig adds `jig.toml` with `backend = "cmake"`. It does not change your files. Your `CMakeLists.txt` is run with the pinned compiler, so the build uses the toolchain everyone else uses.

## Using the machine's compiler

```toml
[toolchain]
vendor      = "system"
hermeticity = "h0"
```

You give up pinning. `jig doctor` says so every time.

## Dependencies *(v0.2)*

```toml
[dependencies]
fmt = { vcpkg = "11" }
```

A v0.1 jig rejects this section with `DJ1006`.
