# Error codes

Every jig error has a stable code `DJxxxx`. Codes never change meaning once released (decision D-008). Each heading below is the anchor used by the `docs:` line in error output (`https://djet.online/jig/docs/errors#DJ2101`).

Ranges: `DJ1xxx` project files, `DJ2xxx` toolchains, `DJ3xxx` environment, `DJ4xxx` build, `DJ5xxx` network and store, `DJ6xxx` init, `DJ7xxx` distribution and self-update, `DJ8xxx` doctor, `DJ9xxx` internal and dependencies.

Exit codes: 1 failure, 2 usage, 3 project, 4 toolchain, 5 network, 6 integrity, 7 build, 8 test, 101 bug, 130 interrupted.

| Code | Exit | Phase | Summary | What to do |
|---|---|---|---|---|
| <a id="DJ1001"></a>DJ1001 | 3 | 003 | No `jig.toml` found in this directory or any parent. | Run `jig init`, or pass `--manifest-path`. |
| <a id="DJ1002"></a>DJ1002 | 3 | 003 | `jig.toml` is not valid TOML. | Fix the syntax at the reported line and column. |
| <a id="DJ1003"></a>DJ1003 | 3 | 003 | Unknown key in `jig.toml`. | Use the suggested key, or `--allow-unknown-keys` for tool extensions. |
| <a id="DJ1004"></a>DJ1004 | 3 | 003 | A key has the wrong type. | See `reference/manifest.md`. |
| <a id="DJ1005"></a>DJ1005 | 3 | 003 | Invalid project name. | Use `[a-z][a-z0-9_-]{0,63}`. |
| <a id="DJ1006"></a>DJ1006 | 3 | 003 | Section reserved for a later release (`[dependencies]` before v0.2). | Remove it, or wait for v0.2. |
| <a id="DJ1007"></a>DJ1007 | 3 | 003 | `std` is not valid for `language`. | Choose a C standard for C, a C++ standard for C++. |
| <a id="DJ1008"></a>DJ1008 | 3 | 003 | Invalid version specification. | See the grammar in `blueprint/003`. |
| <a id="DJ1102"></a>DJ1102 | 3 | 003 | `jig.lock` is missing and `--locked` or `--frozen` was given. | Run `jig lock` and commit the file. |
| <a id="DJ1103"></a>DJ1103 | 3 | 003 | `jig.lock` is stale and `--locked` or `--frozen` was given. | Run `jig lock`. |
| <a id="DJ1104"></a>DJ1104 | 3 | 003 | `jig.lock` was written by a newer format. | Run `jig self update`. |
| <a id="DJ1105"></a>DJ1105 | 3 | 003 | `jig.lock` is malformed. | Delete it and run `jig lock`, or restore it from version control. |
| <a id="DJ2101"></a>DJ2101 | 4 | 004 | No toolchain satisfies the requested version. | Pick a listed version: `jig toolchain available`. |
| <a id="DJ2102"></a>DJ2102 | 4 | 004 | Versions match, but none accepts the project's `std`. | Use a newer compiler or a lower standard. |
| <a id="DJ2201"></a>DJ2201 | 4 | 004 | No system compiler found. | Install one, or use `vendor = "llvm"`. |
| <a id="DJ2203"></a>DJ2203 | 4 | 004 | A required tool (CMake) was not found on PATH. | Install CMake 3.20 or newer. |
| <a id="DJ2202"></a>DJ2202 | 4 | 004 | A system compiler was found but did not respond correctly. | Run with `-vv` to see the probe. |
| <a id="DJ2301"></a>DJ2301 | 4 | 003 | Hermeticity of the current toolchain is below the project's required minimum. | Lower `[toolchain] hermeticity`, or switch vendor. |
| <a id="DJ2401"></a>DJ2401 | 4 | 004 | The index entry was revoked. | Choose another version; the message states the reason. |
| <a id="DJ3004"></a>DJ3004 | 1 | 001 | A path cannot be represented as UTF-8. | Move the project to a path jig can represent. |
| <a id="DJ3101"></a>DJ3101 | 4 | 005 | A shim reports a different version than the lock. | Run `jig sync --reinstall`. |
| <a id="DJ3102"></a>DJ3102 | 1 | 005 | Building the environment failed; the previous one was kept. | Read the cause line; retry with `-vv`. |
| <a id="DJ3103"></a>DJ3103 | 3 | 005 | The environment is not in sync with the lock (`sync --check`). | Run `jig sync`. |
| <a id="DJ3201"></a>DJ3201 | 4 | 005 | Visual Studio Build Tools were not found. | Install "Desktop development with C++" (MSVC and Windows SDK). |
| <a id="DJ3202"></a>DJ3202 | 4 | 005 | macOS SDK was not found. | Run `xcode-select --install`. |
| <a id="DJ3203"></a>DJ3203 | 4 | 005 | A host requirement of the toolchain is not met (for example glibc too old). | Choose an older toolchain or a newer host. |
| <a id="DJ4001"></a>DJ4001 | 7 | 007 | CMake configure failed. | Read the last lines of CMake output. |
| <a id="DJ4002"></a>DJ4002 | 7 | 007 | The build failed. | Fix the first failing command shown. |
| <a id="DJ4003"></a>DJ4003 | 1 | 007 | No executable target to run. | Check `kind` or add a target. |
| <a id="DJ4004"></a>DJ4004 | 2 | 007 | Several executable targets; name one. | `jig run <target>`. |
| <a id="DJ4005"></a>DJ4005 | 8 | 007 | Tests failed. | See the failing test output. |
| <a id="DJ4007"></a>DJ4007 | 1 | 007 | `clang-format` was not found (or failed). | Install clang-format. |
| <a id="DJ4008"></a>DJ4008 | 7 | 007 | No source files match `build.sources`. | Add source files, or change `[build] sources`. |
| <a id="DJ4006"></a>DJ4006 | 8 | 007 | `fmt --check` found differences. | Run `jig fmt`. |
| <a id="DJ4101"></a>DJ4101 | 2 | 007 | Profile not found. | List profiles in `jig.toml`. |
| <a id="DJ4102"></a>DJ4102 | 0 | 007 | An option is not supported by this compiler (warning). | Remove the option or change the compiler. |
| <a id="DJ4103"></a>DJ4103 | 0 | 007 | A source glob matched no files (warning). | Check `[build] sources`. |
| <a id="DJ5001"></a>DJ5001 | 5 | 004 | Offline, and the needed item is not in the cache or store. | Run once online, or point `--mirror` at a copy. |
| <a id="DJ5002"></a>DJ5002 | 5 | 004 | HTTP error. | The message has the status and URL. |
| <a id="DJ5003"></a>DJ5003 | 5 | 004 | Network timeout. | Retry; check proxy settings. |
| <a id="DJ5101"></a>DJ5101 | 6 | 004 | The index signature is invalid. | Do not continue; check the index URL and report if it persists. |
| <a id="DJ5102"></a>DJ5102 | 6 | 004 | Archive size or sha256 does not match the signed index. | The partial file was deleted; retry, then report. |
| <a id="DJ5103"></a>DJ5103 | 6 | 004 | Unknown or expired signing key. | Run `jig self update`. |
| <a id="DJ5104"></a>DJ5104 | 5 | 004 | The index format is newer than this jig supports. | Run `jig self update`. |
| <a id="DJ5105"></a>DJ5105 | 6 | 004 | An archive failed the safety checks. | Nothing was installed; report the entry id. |
| <a id="DJ6001"></a>DJ6001 | 1 | 006 | The target directory is not empty. | Use `--adopt` or `--force`. |
| <a id="DJ6002"></a>DJ6002 | 2 | 006 | Invalid project name. | See DJ1005. |
| <a id="DJ6003"></a>DJ6003 | 2 | 006 | Non-interactive and required flags are missing. | The message lists every missing flag. |
| <a id="DJ6004"></a>DJ6004 | 101 | 006 | Template rendering failed. | This is a bug; report it with the template name. |
| <a id="DJ6005"></a>DJ6005 | 0 | 006 | `git` was requested but not found (warning). | Install git or use `--vcs none`. |
| <a id="DJ6006"></a>DJ6006 | 1 | 006 | Adopt mode found no `CMakeLists.txt`. | Run plain `jig init`. |
| <a id="DJ7001"></a>DJ7001 | 1 | 010 | `self update` refused: installed by a package manager. | Update with that package manager. |
| <a id="DJ7002"></a>DJ7002 | 4 | 010 | No `self` entry for this host. | Download manually. |
| <a id="DJ7003"></a>DJ7003 | 6 | 010 | Self-update signature or hash failure. | Nothing was replaced; report it. |
| <a id="DJ7004"></a>DJ7004 | 1 | 010 | The running binary could not be replaced. | Close other jig processes; check permissions. |
| <a id="DJ8001"></a>DJ8001 | 0 | 008 | Unsupported platform (warning). | See `platforms.md`. |
| <a id="DJ8002"></a>DJ8002 | 1 | 008 | `JIG_HOME` is not writable or low on space. | Free space or set `JIG_HOME`. |
| <a id="DJ8003"></a>DJ8003 | 1 | 008 | Doctor found problems under `--strict`. | Fix the listed findings. |
| <a id="DJ9001"></a>DJ9001 | 1 | 001 | Not implemented in this build (a feature that is designed but not yet written). | See `CODE-STATUS.md`. |
| <a id="DJ9000"></a>DJ9000 | 101 | 008 | Internal error. | Run `jig doctor --report` and file an issue. |
| <a id="DJ9101"></a>DJ9101 | 0 | 009 | A git dependency tracks a branch (warning, v0.2). | Use a tag or `rev`. |
| <a id="DJ9102"></a>DJ9102 | 3 | 009 | Dependency conflict (v0.2). | See the requirement chain printed. |
| <a id="DJ9103"></a>DJ9103 | 3 | 009 | Dependency needs a language standard the project does not use (v0.2). | Raise `std` or choose another version. |
| <a id="DJ9104"></a>DJ9104 | 7 | 009 | A vcpkg port failed to build (v0.2). | Read the port log path printed. |
| <a id="DJ9105"></a>DJ9105 | 3 | 009 | Package not found in the baseline (v0.2). | Check the name; `jig search`. |

## Message shape

```
✗ DJ2101  No toolchain satisfies "clang@25" for this platform

  <facts, not blame>
  help: <a runnable command>
  docs: https://djet.online/jig/docs/errors#DJ2101
```
