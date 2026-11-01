# Hermeticity

A build is **hermetic** when it depends only on what the project declares. Real builds are never perfectly hermetic. jig tells you where yours stands.

```
$ jig doctor
...
Hermeticity            H1  pinned compiler            (required: h1)
  pinned               clang 20.1.8 · cmake 3.31.2 · ninja 1.12.1
  from the host        glibc · system headers · libstdc++
  to reach H2          needs jig 0.3 (pinned C++ runtime)
```

(Illustrative.)

## The levels

| Level | Pinned by hash | Still from the machine | Available |
|---|---|---|---|
| **H0 Detected** | nothing | everything | 0.1 |
| **H1 Pinned compiler** | compiler, CMake, Ninja | C library, system headers, platform SDK, and on some platforms the C++ runtime | 0.1 |
| **H2 Pinned runtime** | H1 plus the compiler's C++ runtime and a sysroot where the provider ships one | kernel interface, macOS and Windows SDKs | 0.3 |
| **H3 Hermetic** | H2, able to build for a target from any host | kernel | 0.3 |

## Why not claim more

Most "reproducible build" claims quietly depend on the host's C library and headers. jig names them. If your project must build identically on machines with different glibc versions, H1 will not be enough and `doctor` says so.

## Requiring a level

```toml
[toolchain]
hermeticity = "h1"
```

A build on a toolchain below that level fails with `DJ2301` instead of proceeding quietly. A project that uses `vendor = "system"` sets `h0` and accepts the trade.

## What jig never does

- It does not claim isolation it cannot verify.
- It does not run any file from a downloaded archive during install.
- It does not use an unsigned index or an unverified archive.
- It does not send data anywhere. There is no telemetry in 0.1.
