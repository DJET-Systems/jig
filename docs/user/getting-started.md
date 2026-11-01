# Getting started

## 1. Install

Linux and macOS:

```
$ curl -fsSL https://djet.online/jig/install.sh | sh
```

Windows (PowerShell):

```
> irm https://djet.online/jig/install.ps1 | iex
```

Both put `jig` in a per-user folder and never need administrator rights. The script prints every URL it uses, checks the download against a published checksum, and tells you if your `PATH` needs a line added. To read it first, download it, read it, run it. To install by hand, see [Distribution](../blueprint/010-distribution-and-release.md) section 4.

Check it:

```
$ jig --version
jig 0.1.0
$ jig doctor
```

**Before you build on macOS or Windows:** macOS needs the command line tools (`xcode-select --install`). Windows needs Visual Studio Build Tools with the "Desktop development with C++" workload. `jig doctor` tells you if either is missing. Linux needs nothing beyond a standard C library.

## 2. Create a project

```
$ jig init hello
```

jig asks six short questions. The important one comes second: **the standard**, and then the **compiler version**, filtered to versions that accept it.

```
? Language     › C++
? Standard     › c++23
? Compiler     › clang (llvm, pinned)
? Version      › 20.1.8   (latest 20.x)
? Project kind › executable
? Initialise git › yes
```

To skip every question, give the answers as flags:

```
$ jig init hello --lang c++ --std c++23 --toolchain clang@20 -y
```

jig then downloads the compiler once (it is shared by all your projects), writes the project, writes `jig.lock`, and creates `.jig/env`.

```
hello/
  jig.toml         what you want
  jig.lock         exactly what you got (commit this)
  src/main.cpp
  tests/smoke.cpp
  .clang-format
  .gitignore
  README.md
  .jig/            environment and build output (never commit)
```

## 3. Build and run

```
$ cd hello
$ jig run
▸ Building               3 targets
  ✓ built in 1.9 s
hello from hello
```

```
$ jig test
$ jig build --release
$ jig fmt --check
```

## 4. See what jig did

Every command narrates. For the reasoning behind each choice:

```
$ jig lock --explain
```

For every command line it ran:

```
$ jig build -vv
```

For machine-readable output:

```
$ jig build --json
```

## 5. Use the environment yourself

Run anything with the pinned tools on `PATH`:

```
$ jig exec -- c++ --version
$ jig exec -- cmake --version
```

Or work inside it:

```
$ jig shell
(jig) $ c++ --version
```

Or activate it in your current shell and leave it with `deactivate`:

```
$ source .jig/env/activate
```

## 6. Change the compiler

```
$ jig toolchain available --lang c++ --std c++23
$ jig toolchain use clang@19
$ jig build
```

The manifest and lock change; the next build uses the new compiler. Your teammates get the same one when they pull.

## 7. Work with a team

Commit `jig.toml` and `jig.lock`. A teammate runs:

```
$ git clone <repo> && cd <repo>
$ jig build
```

jig sees there is no environment, creates it from the lock, installs the exact compiler by hash, and builds. In CI use `jig sync --frozen && jig build --locked` so a stale lock fails the job instead of changing the build.

## Next

[Concepts](concepts.md) · [Toolchains](toolchains.md) · [Projects and the manifest](projects.md)
