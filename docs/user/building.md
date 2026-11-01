# Building

jig generates a CMake project and builds it with Ninja, using the compiler from the environment. You never write the CMake unless you choose to.

## The commands

| Command | Does |
|---|---|
| `jig build` | Configure (if needed) and build. |
| `jig build --release` | The `release` profile. |
| `jig run [target] -- args` | Build, then run an executable. |
| `jig test [filter]` | Build, then run the tests with CTest. |
| `jig fmt` / `jig fmt --check` | Format with the pinned `clang-format`; `--check` fails if something would change. |
| `jig clean` | Remove build output. `--env` also removes the environment. |

## What a build looks like

```
$ jig build
▸ Reading project          hello 0.1.0 · c++23 · exe · profile debug
▸ Environment              in sync
▸ Generating               .jig/gen/CMakeLists.txt unchanged
▸ Configuring              cache valid, skipped
▸ Building                 3 targets · -j 12
  [3/3] Linking CXX executable hello
  ✓ built in 1.9 s
```

(Illustrative.) Configure is skipped when nothing that affects it has changed, so a second build only runs Ninja.

## Where things go

| Path | Contents |
|---|---|
| `.jig/gen/` | The generated CMake project. Never edit. |
| `.jig/build/<profile>/` | One build tree per profile. |
| `compile_commands.json` | Linked into the project root for clangd and other tools. |

## Failure

A compile error exits with code 7 and prints the first failing command, even in quiet mode. Test failures exit with code 8. `jig build -vv` shows every command that was run.

## Using your own CMake

With `[build] backend = "cmake"`, jig runs your `CMakeLists.txt`, passes the toolchain file and maps the profile to `CMAKE_BUILD_TYPE` and flags. Extra arguments go in `[build] cmake-args`.

## Windows

jig uses the Ninja generator and the compiler from the environment. It does not use Visual Studio solution generators. Visual Studio Build Tools must be installed for the runtime and SDK (`DJ3201` explains if not).

## In CI

```
jig sync --frozen
jig build --locked --release
jig test   --locked
jig fmt    --check
jig doctor --strict --json > doctor.json
```

`--frozen` and `--locked` make a stale lock a failure, so CI never silently changes the toolchain.
