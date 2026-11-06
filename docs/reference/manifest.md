# Manifest reference and sample sheets

The contract for `jig.toml`, `jig.lock` and `config.toml`. Unknown keys are errors (`DJ1003`). A JSON Schema is generated from the parser by a test (T-003-03) into `jig.schema.json` next to this file; if this sheet and the schema disagree, CI fails.

## Key index

| Path | Type | Default | Notes |
|---|---|---|---|
| `project.name` | string | directory name | `[a-z][a-z0-9_-]{0,63}` |
| `project.version` | string | `0.1.0` | SemVer |
| `project.description` | string | `""` | |
| `project.license` | string | `""` | SPDX |
| `project.authors` | string[] | `[]` | |
| `project.kind` | enum | `exe` | `exe` `static-lib` `shared-lib` `header-only` |
| `project.language` | enum | required | `c` `c++` |
| `project.std` | string | required | `c11 c17 c23` `c++14 c++17 c++20 c++23 c++26` |
| `project.extensions` | bool | `false` | `gnu` dialect when true |
| `project.platforms` | string[] | current + tier 1 | host triples |
| `toolchain.vendor` | enum | `llvm` | `llvm` `system` |
| `toolchain.version` | spec | `latest` | `20`, `20.1`, `20.1.8`, `^20`, `~20.1`, `>=18,<21`, `latest` |
| `toolchain.hermeticity` | enum | `h1` (llvm), `h0` (system) | minimum level |
| `toolchain.tools.cmake` | spec | `>=3.20,<5` | |
| `toolchain.tools.ninja` | spec | `1` | |
| `toolchain.tools.clang-format` | spec or `"bundled"` | `"bundled"` | |
| `toolchain.system.cc`, `.cxx` | string | detected | only with `vendor = "system"` |
| `build.backend` | enum | `generated` | `generated` `cmake` `none` |
| `build.sources` | glob[] | `["src/**/*.{c,cc,cpp,cxx}"]` | |
| `build.include` | path[] | `["include","src"]` | |
| `build.public-include` | path[] | `["include"]` | libraries |
| `build.defines` | string[] | `[]` | `NAME` or `NAME=VALUE` |
| `build.warnings` | enum | `default` | `off` `default` `strict` |
| `build.link` | string[] | `[]` | system libraries |
| `build.cmake-args` | string[] | `[]` | `cmake` backend only |
| `profile.<n>.opt` | enum | `"0"` debug, `"3"` release | `0 1 2 3 s z` |
| `profile.<n>.debug` | bool | `true` / `false` | |
| `profile.<n>.lto` | `false`/`"thin"`/`"full"` | `false` / `"thin"` | |
| `profile.<n>.sanitizers` | string[] | `[]` | `address undefined thread memory` |
| `profile.<n>.assertions` | bool | `true` / `false` | |
| `profile.<n>.defines` | string[] | `[]` | |
| `profile.<n>.inherits` | string | | another profile |
| `target[].name` | string | | |
| `target[].kind` | enum | | as `project.kind` |
| `target[].sources` | glob[] | | |
| `target[].link-targets` | string[] | | targets in this project |
| `test.sources` | glob[] | `["tests/**/*.{c,cc,cpp,cxx}"]` | one executable per file |
| `test.runner` | enum | `ctest` | |
| `test.args` | string[] | `["--output-on-failure"]` | |
| `fmt.style` | string | `file` | or `llvm` `google` `mozilla` `webkit` |
| `env.inherit` | enum | `minimal` | `none` `minimal` `all` |
| `env.set` | table | `{}` | |
| `dependencies` | table | | **reserved, v0.2** (`DJ1006`) |

## Sample sheet

### S1. C++23 executable (what `jig init` writes)

```toml
[project]
name     = "hello"
version  = "0.1.0"
kind     = "exe"
language = "c++"
std      = "c++23"

[toolchain]
vendor  = "llvm"
version = "20"

[test]
sources = ["tests/**/*.cpp"]
```

### S2. C17 static library, strict warnings, sanitizers in debug

```toml
[project]
name     = "ringbuf"
version  = "1.2.0"
license  = "AGPL-3.0-only"
kind     = "static-lib"
language = "c"
std      = "c17"

[toolchain]
vendor      = "llvm"
version     = "^19"
hermeticity = "h1"

[build]
warnings       = "strict"
public-include = ["include"]

[profile.debug]
sanitizers = ["address", "undefined"]

[profile.release]
lto = "thin"
```

### S3. Header-only C++20 library

```toml
[project]
name     = "smallvec"
kind     = "header-only"
language = "c++"
std      = "c++20"

[toolchain]
version = ">=17,<21"
```

### S4. Existing CMake project, adopted

```toml
[project]
name     = "legacy-app"
kind     = "exe"
language = "c++"
std      = "c++17"

[toolchain]
vendor  = "llvm"
version = "18.1"

[build]
backend    = "cmake"
cmake-args = ["-DLEGACY_FEATURE=ON"]
```

### S5. Use the machine's compiler (H0)

```toml
[project]
name     = "quick"
kind     = "exe"
language = "c"
std      = "c11"

[toolchain]
vendor      = "system"
hermeticity = "h0"

[toolchain.system]
cc = "gcc"
```

### S6. Several targets in one project

```toml
[project]
name     = "suite"
kind     = "exe"
language = "c++"
std      = "c++23"

[[target]]
name    = "suite-core"
kind    = "static-lib"
sources = ["src/core/**/*.cpp"]

[[target]]
name         = "suite-cli"
kind         = "exe"
sources      = ["src/cli/**/*.cpp"]
link-targets = ["suite-core"]
```

### S7. Environment control

```toml
[env]
inherit = "none"
set     = { SUITE_MODE = "ci", TZ = "UTC" }
```

### S8. User `config.toml`

```toml
[ui]
verbosity = 2
color     = "auto"
glyphs    = "auto"

[network]
offline   = false
index-url = "https://djet.online/jig/index/v1/"
mirror    = ""

[defaults]
language = "c++"
std      = "c++23"
vendor   = "llvm"
vcs      = "git"
```

### S9. `jig.lock` (shape)

```toml
version = 1

[meta]
generated-by    = "jig 0.1.0"
index-digest    = "sha256:…"
manifest-digest = "sha256:…"

[[toolchain]]
vendor = "llvm"
version = "20.1.8"
host = "x86_64-unknown-linux-gnu"
id = "llvm-20.1.8-x86_64-unknown-linux-gnu"
url = "https://…"
sha256 = "…"
size = 912345678
provides = ["clang", "clang++", "clang-format", "clang-tidy", "clangd", "lld", "llvm-ar"]
hermeticity = "h1"

[[tool]]
name = "cmake"
version = "3.31.2"
host = "x86_64-unknown-linux-gnu"
sha256 = "…"
url = "https://…"

[[tool]]
name = "ninja"
version = "1.12.1"
host = "x86_64-unknown-linux-gnu"
sha256 = "…"
url = "https://…"
```

(Versions, sizes, hashes and URLs in samples are placeholders.)

## Common mistakes

| Mistake | Result | Fix |
|---|---|---|
| `std = "c++23"` with `language = "c"` | `DJ1007` | Match them. |
| `version = "20"` and `std = "c++26"` where 20 does not accept it | `DJ2102` at lock time | Newer compiler or lower std. |
| Misspelled `warnigns` | `DJ1003` with "did you mean `warnings`" | Fix the key. |
| `[dependencies]` in v0.1 | `DJ1006` | Wait for v0.2. |
| Editing `jig.lock` by hand | Digest mismatch, stale lock | Run `jig lock`. |
