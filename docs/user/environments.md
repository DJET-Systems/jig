# Environments

Every project has an environment in `.jig/env`. It is built from `jig.lock`, it can be deleted at any time, and `jig sync` recreates it exactly.

## What is inside

```
.jig/env/
  bin/              cc  c++  clang  clang++  clang-format  clang-tidy  clangd  lld  llvm-ar  cmake  ctest  ninja
  toolchain.cmake   tells CMake which compilers to use
  env.json          the resolved variables
  activate          bash, zsh
  activate.fish
  activate.ps1
  activate.bat
```

The `bin/` entries are links (or, on Windows, tiny launchers) that point into the shared store. The environment holds no copy of the compiler.

## Ways to use it

| You want | Do |
|---|---|
| Run one command | `jig exec -- c++ --version` |
| A shell with everything set | `jig shell` |
| Activate the shell you are in | `source .jig/env/activate` or `eval "$(jig env)"` |
| PowerShell | `jig env --format pwsh \| Invoke-Expression` or `.jig\env\activate.ps1` |
| Leave an activated shell | `deactivate` |
| Make an editor use the right compiler | Point it at `compile_commands.json` (jig links it into the project root) |

Commands like `build`, `run` and `test` use the environment automatically and sync it first if needed.

## What jig sets

`JIG_ENV`, `JIG_PROJECT`, `PATH` (with `.jig/env/bin` first), `CC`, `CXX`, `AR`, `CMAKE_GENERATOR=Ninja`, `CMAKE_TOOLCHAIN_FILE`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, and on macOS `SDKROOT`. On Windows it also captures the Visual Studio developer environment when the compiler needs it. The full list is in [Environment variables](../reference/env-vars.md).

## What jig removes

By default (`[env] inherit = "minimal"`), variables that commonly leak the machine into a build are dropped: `CFLAGS`, `CXXFLAGS`, `CPPFLAGS`, `LDFLAGS`, `CPATH`, `C_INCLUDE_PATH`, `CPLUS_INCLUDE_PATH`, `LIBRARY_PATH`, `LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`, `CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH`. `jig doctor` lists the names it dropped.

| `[env] inherit` | Keeps from your shell |
|---|---|
| `none` | Home, temp, terminal and locale only |
| `minimal` (default) | The above, later `PATH` entries, proxy variables |
| `all` | Everything (lowers confidence; `doctor` says so) |

Set your own variables with `[env] set = { NAME = "value" }`.

## Keeping it in sync

```
$ jig sync               make the environment match the lock
$ jig sync --check       exit 3 if it does not (for CI), change nothing
$ jig sync --reinstall   rebuild it from scratch
```

Sync is atomic: the new environment is built beside the old one and swapped in. If it fails, you keep the old one.

## Two projects, one compiler

Both environments link to the same copy in the store. Removing a project's `.jig/` does not remove the compiler. To reclaim space, `jig cache prune` (a dry run by default) removes store entries that no known lock still needs.
