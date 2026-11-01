# Environment variables

## Read by jig

| Variable | Equivalent flag / key | Meaning |
|---|---|---|
| `JIG_HOME` | `--home`, `store.home` | User data directory (store, cache, index, logs). |
| `JIG_CONFIG_DIR` | | Directory holding `config.toml`. |
| `JIG_MANIFEST` | `--manifest-path` | Path to `jig.toml`. |
| `JIG_VERBOSE` | `-v` ... | `1` to `3`. |
| `JIG_QUIET` | `-q` ... | `1` to `2`. |
| `JIG_TERSE` | `--terse` | `1` enables. |
| `JIG_JSON` | `--json` | `1` enables. |
| `JIG_COLOR` | `--color` | `auto`, `always`, `never`. |
| `NO_COLOR` | `--color never` | Any non-empty value disables colour. |
| `JIG_OFFLINE` | `--offline` | `1` enables. |
| `JIG_LOCKED` | `--locked` | `1` enables. |
| `JIG_FROZEN` | `--frozen` | `1` enables. |
| `JIG_YES` | `-y` | `1` enables. |
| `JIG_NO_INTERACTIVE` | `--no-interactive` | `1` enables. |
| `JIG_INDEX_URL` | `network.index-url` | Index base URL. The signature is still verified. |
| `JIG_MIRROR` | `network.mirror` | Directory or URL that mirrors the index and archives. |
| `HTTPS_PROXY`, `HTTP_PROXY`, `NO_PROXY` | `network.proxy` | Standard proxy variables. |
| `SSL_CERT_FILE`, `SSL_CERT_DIR` | | Alternative CA bundle. |
| `SHELL`, `ComSpec` | `--shell` | Used by `jig shell`. |
| `JIG_UPDATE_GOLDEN` | | Tests only: rewrite golden files. |

Precedence: flag, then variable, then `jig.toml`, then `config.toml`, then default.

## Set by jig inside the environment

| Variable | Value |
|---|---|
| `JIG_ENV` | Absolute path to `.jig/env`. |
| `JIG_PROJECT` | Absolute path to the project root. |
| `PATH` | `.jig/env/bin` first. |
| `CC`, `CXX`, `AR` | Shims inside `.jig/env/bin`. |
| `CMAKE_GENERATOR` | `Ninja`. |
| `CMAKE_TOOLCHAIN_FILE` | `.jig/env/toolchain.cmake`. |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | `ON`. |
| `SDKROOT` | macOS SDK path (macOS only). |
| `LIB`, `INCLUDE`, `LIBPATH`, ... | Visual Studio developer environment (Windows, when needed). |

## Removed by jig under `[env] inherit = "none"` or `"minimal"`

`CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `CPPFLAGS`, `LDFLAGS`, `CPATH`, `C_INCLUDE_PATH`, `CPLUS_INCLUDE_PATH`, `LIBRARY_PATH`, `LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`, `CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH`.

`jig doctor` lists the **names** (never the values) of any that were present and dropped.
