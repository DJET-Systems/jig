# CLI reference

This sheet is the contract for the command surface. A test (T-002-02) compares it with the CLI11 command tree and fails when they differ. Entries marked **(v0.2)** are reserved.

## Synopsis

```
jig [GLOBAL FLAGS] <COMMAND> [ARGS]
```

## Global flags

| Flag | Env | Description |
|---|---|---|
| `-v`, `-vv`, `-vvv` | `JIG_VERBOSE` | Detail, commands, trace. |
| `-q`, `-qq` | `JIG_QUIET` | Terse, errors only. |
| `--terse` | `JIG_TERSE` | One line per step. |
| `--explain` | | Print the decision tree after the output. |
| `--json` | `JIG_JSON` | Newline-delimited JSON events on stdout. |
| `--color auto\|always\|never` | `JIG_COLOR`, `NO_COLOR` | Colour. |
| `--offline` | `JIG_OFFLINE` | No network. |
| `--locked` | `JIG_LOCKED` | Fail if the lock is missing or stale. |
| `--frozen` | `JIG_FROZEN` | `--locked` plus `--offline`. |
| `--manifest-path PATH` | `JIG_MANIFEST` | Use another `jig.toml`. |
| `--home DIR` | `JIG_HOME` | User data directory. |
| `-y`, `--yes` | `JIG_YES` | Accept defaults and confirmations. |
| `--no-interactive` | `JIG_NO_INTERACTIVE` | Never prompt. |
| `-h`, `--help` | | Help. |
| `-V`, `--version` | | Version. |

---

## `jig init [PATH]`

Create a project. Asks for anything not given by a flag.

| Option | Description |
|---|---|
| `--name NAME` | Project name (default: directory name). |
| `--lang c\|c++` | Language. |
| `--std STD` | `c11 c17 c23` or `c++14 c++17 c++20 c++23 c++26`. |
| `--vendor llvm\|system` | Compiler vendor. |
| `--toolchain SPEC` | For example `clang@20`, `clang@20.1`, `clang@>=18,<21`, `clang@latest`. |
| `--compiler-first` | Ask for the compiler before the standard. |
| `--prerelease` | Include prerelease compilers. |
| `--allow-incompatible` | Continue with a compiler that does not accept the standard (warns). |
| `--kind exe\|static-lib\|shared-lib\|header-only` | Project kind. |
| `--tests none\|ctest` | Test scaffold. (`catch2`, `doctest`, `gtest`, `unity` in v0.2.) |
| `--clang-format none\|llvm\|google\|mozilla\|webkit\|file` | `.clang-format`. |
| `--vcs git\|none` | `git init` and `.gitignore`. |
| `--license SPDX` | Recorded in `jig.toml`. |
| `--platforms LIST` | Hosts the lock must cover. |
| `--template NAME` | Force a template (see blueprint 006). |
| `--adopt` | Add `jig.toml` to an existing CMake project. |
| `--force` | Write into a non-empty directory. |
| `--no-install`, `--no-sync`, `--no-lock` | Skip steps. |
| `--dry-run` | Print decisions and file list; write nothing. |

```
jig init hello --lang c++ --std c++23 --toolchain clang@20 -y
jig init --adopt --lang c++ --std c++20 --toolchain clang@19
jig init lib --lang c --std c17 --kind static-lib --vendor system --dry-run -y
```

## `jig toolchain`

| Subcommand | Synopsis | Notes |
|---|---|---|
| `list` | `jig toolchain list [--all]` | Installed in the store. `--all` includes detected system compilers. |
| `available` | `jig toolchain available [--vendor V] [--lang L] [--std S] [--host TRIPLE] [--prerelease]` | From the index, filtered. |
| `install` | `jig toolchain install SPEC [--vendor V] [--host TRIPLE]` | Download, verify, extract. |
| `use` | `jig toolchain use SPEC` | Edit `jig.toml`, re-lock, sync. |
| `pin` | `jig toolchain pin [SPEC]` | Write the exact version into `jig.toml`. Without `SPEC`, pins the currently locked one. |
| `which` | `jig toolchain which [TOOL]` | Print the real path of a tool. |
| `remove` | `jig toolchain remove SPEC [--force]` | Remove from the store. |
| `update` | `jig toolchain update` | Refresh the index; report newer patch versions. |

```
jig toolchain available --lang c++ --std c++23
jig toolchain install clang@20
jig toolchain use clang@19.1
```

## `jig lock`

`jig lock [--platform HOST...] [--upgrade] [--check]`. Resolve and write `jig.lock`. `--check` exits 3 if the lock would change. `--platform all` covers every supported host.

## `jig sync`

`jig sync [--check] [--reinstall]`. Make `.jig/env` match the lock.

## `jig env`

`jig env [--format sh|fish|pwsh|cmd|json]`. Print the environment.

```
eval "$(jig env)"
jig env --format pwsh | Invoke-Expression
```

## `jig shell`

`jig shell [--shell NAME]`. Subshell with the environment.

## `jig exec`

`jig exec -- CMD [ARGS...]`. Run any command inside the environment.

```
jig exec -- c++ --version
jig exec -- cmake --version
```

## `jig build`

| Option | Description |
|---|---|
| `--profile NAME` | Build profile (default `debug`). |
| `--release` | Same as `--profile release`. |
| `--target NAME` | Build one target. |
| `-j N`, `--jobs N` | Parallel jobs (default: logical CPUs). |
| `--no-compile-commands` | Do not link `compile_commands.json`. |
| `--configure-only` | Stop after configure. |

## `jig run`

`jig run [TARGET] [--profile NAME] [--release] -- [ARGS...]`. Build and run an executable.

## `jig test`

`jig test [FILTER] [--profile NAME] [-- CTEST_ARGS...]`. Build and run tests. Exit 8 on failure.

## `jig fmt`

`jig fmt [--check] [PATHS...]`. Format with the pinned `clang-format`.

## `jig clean`

`jig clean [--profile NAME] [--env] [--all]`.

## `jig doctor`

`jig doctor [--online] [--features] [--fix] [--strict] [--report] [--json]`.

## `jig cache`

| Subcommand | Notes |
|---|---|
| `dir` | Print `JIG_HOME`. |
| `size` | Sizes by area. |
| `prune [--apply] [--keep-days N]` | Dry run by default. |
| `clean --downloads` | Remove the download cache. |

## `jig self`

| Subcommand | Notes |
|---|---|
| `update [--check] [--version X]` | Verified update. |
| `verify` | Check this binary against the signed release data. |
| `rollback` | Restore the previous version. |
| `where` | Print the path of the running binary. |

## `jig completions SHELL`

`bash`, `zsh`, `fish`, `powershell`, `elvish`.

## `jig version`

Version, commit, build date, index format, supported manifest and lock versions.

## Reserved (v0.2)

`jig add`, `jig remove`, `jig tree`, `jig search`, `jig check`.
