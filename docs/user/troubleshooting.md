# Troubleshooting

Start with:

```
$ jig doctor
$ jig <failing command> -vv
$ jig <failing command> --explain
```

## Find your symptom

| Symptom | Likely cause | Fix |
|---|---|---|
| `DJ1001 No jig.toml found` | You are outside a project. | `cd` into it, or `jig init`. |
| `DJ1003 Unknown key` | Typo in `jig.toml`. | Use the suggested key. |
| `DJ1103 jig.lock is stale` | Manifest changed under `--locked`. | `jig lock`, commit the lock. |
| `DJ2101 No toolchain satisfies …` | Version not in the index for this host. | `jig toolchain available`. |
| `DJ2102 … accepts up to c++20` | Compiler too old for the standard. | Newer compiler or lower `std`. |
| `DJ3201 Visual Studio Build Tools not found` | Windows needs the runtime and SDK. | Install "Desktop development with C++". |
| `DJ3202 macOS SDK not found` | No command line tools. | `xcode-select --install`. |
| `DJ3203 host requirement not met` | Older glibc than the toolchain needs. | Use an older toolchain or newer host. |
| `DJ5001 offline` | Needed item not cached. | Run once online or use a mirror. |
| `DJ5101 / DJ5102` | Signature or hash failed. | Do not bypass. Retry; if it persists, report it. |
| `DJ4002 build failed` | Compile error. | Fix the first failing command shown. |
| `command not found: gcc` inside `jig shell` | The environment hides tools it does not pin. | Use `c++`/`clang++`, or `vendor = "system"`. |
| Editor shows wrong includes | Not using `compile_commands.json`. | Run `jig build` once; reload the editor. |
| CI fails with a stale lock | Someone changed the manifest without relocking. | `jig lock` locally and commit. |
| Build differs between two machines | Different host libraries (H1). | Compare `jig doctor --report` from both. |
| A leaked variable changes the build | `inherit = "all"` or a variable not in the drop list. | `inherit = "none"`; see [Environments](environments.md). |

## An install was interrupted

```
$ jig doctor --fix
```

removes incomplete store directories after listing them. Installs are atomic, so an interrupted one never becomes usable.

## Reset

| Goal | Command |
|---|---|
| Rebuild the environment | `jig sync --reinstall` |
| Remove build output | `jig clean` |
| Remove build output and environment | `jig clean --env` |
| Free store space (dry run first) | `jig cache prune`, then `--apply` |

## Report a problem

```
$ jig doctor --report
```

writes `jig-report-<time>.txt` (versions, doctor output, manifest with `[env] set` values redacted, lock, recent log, names of leaked variables). Read it, then attach it to an issue. jig never uploads it.
