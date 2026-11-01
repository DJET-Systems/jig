# FAQ

**Is jig a build system?**
No. It generates CMake and runs Ninja.

**Why not just use CMake?**
CMake builds a project with whatever compiler it finds. jig chooses and pins the compiler, records it in a lock, creates the project, and gives it an isolated environment. CMake still does the building.

**Which compilers can I use?**
Official LLVM/Clang releases (pinned by hash), or a compiler already on your machine (detected, not pinned). Zig `cc` is planned for 0.3 and GCC for 0.4.

**Can I use GCC today?**
Yes, as `vendor = "system"`. It is H0: jig uses it but cannot pin it.

**Does jig install Visual Studio or the macOS SDK?**
No. It checks for them and tells you what to install.

**Does it work offline?**
Anything already in the store works offline. `--offline` makes that a rule. You can also mirror the index and archives to a directory and set `JIG_MIRROR`.

**What does it send over the network?**
Requests for the index and for compiler archives. There is no telemetry.

**How do I know the downloads are genuine?**
The index is signed. Each archive's size and sha256 come from the signed index and are checked after download. A failure stops jig and cannot be switched off.

**Can I check in the `.jig` folder?**
No. It is generated and contains absolute paths. Commit `jig.toml` and `jig.lock`.

**Where are compilers stored?**
In `JIG_HOME` (see [Environment variables](../reference/env-vars.md)). `jig cache dir` prints it and `jig cache size` shows how much space it uses.

**How do I remove a compiler?**
`jig toolchain remove clang@18`. jig refuses if a known lock needs it, unless you pass `--force`.

**How do I upgrade the compiler for a project?**
`jig toolchain use clang@21` (or any spec), then commit the changed `jig.toml` and `jig.lock`.

**Can two projects use different compilers?**
Yes. Each has its own lock and environment.

**Does it work with my editor?**
jig links `compile_commands.json` into the project root, which clangd and most editors read. Run your editor from `jig shell` or point it at `.jig/env/bin/clangd`.

**What about dependencies?**
Version 0.2 adds `jig add` with vcpkg, git and path sources. Until then, vendor your dependencies or use `backend = "cmake"` and manage them in your own CMake.

**Why "jig"?**
A jig is a workshop fixture that holds the work in place and guides the tool. jig holds your project's compiler, standard and build in place the same way, so each build lands where the last one did.

**Who makes it?**
[DJET Systems](https://djet.online).
