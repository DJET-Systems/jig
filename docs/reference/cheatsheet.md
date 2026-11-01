# jig cheat sheet

```
Start
  jig init hello                          wizard
  jig init hello --lang c++ --std c++23 --toolchain clang@20 -y
  jig init --adopt                        add jig to an existing CMake project

Toolchains
  jig toolchain available --std c++23     what can build c++23 here
  jig toolchain install clang@20
  jig toolchain use clang@19.1            change the project's compiler
  jig toolchain which c++                 real path of a pinned tool

Project
  jig lock                                resolve, write jig.lock
  jig sync                                make .jig/env match the lock
  jig build [--release]
  jig run [target] -- args
  jig test [filter]
  jig fmt [--check]
  jig clean [--env]

Environment
  jig exec -- c++ --version               run anything inside the environment
  jig shell                               subshell inside the environment
  eval "$(jig env)"                       activate in the current shell

Truth
  jig doctor [--online] [--features]      report with hermeticity level
  jig <command> --explain                 show the decision tree
  jig <command> -vv                       show every command that was run
  jig <command> --json                    machine-readable events

Safety
  --locked   fail if the lock is stale
  --frozen   --locked plus --offline
  --offline  never touch the network

Files
  jig.toml    intent (you edit)
  jig.lock    exact versions and hashes (jig writes, you commit)
  .jig/       environment and build output (never commit)
```

## Hermeticity

| Level | Pinned | Release |
|---|---|---|
| H0 | nothing (system compiler detected) | v0.1 |
| H1 | compiler, CMake, Ninja by hash | v0.1 |
| H2 | + C++ runtime, sysroot | v0.3 |
| H3 | + any host to any target | v0.3 |

## Exit codes

`0` ok · `1` failure · `2` usage · `3` project · `4` toolchain · `5` network · `6` integrity · `7` build · `8` test · `101` bug · `130` interrupted
