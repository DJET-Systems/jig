# Repository identity

Everything needed to create the repository and its public surfaces. Copy the values as written.

## License

Project-authored material: `AGPL-3.0-only` (`LICENSE`). Vendored dependencies
retain their own terms, summarized in `THIRD_PARTY_NOTICES.md` and documented in
their respective `third_party/*/LICENSE` files.

## Name

| Field | Value |
|---|---|
| Product | **jig** |
| Tagline | **jig, the native C/C++ toolchain.** |
| Binary | `jig` |
| GitHub org / repo | `ansgrb/jig` |
| Product page | `https://djet.online/jig` |
| Docs | `https://djet.online/jig/docs` (rendered from `docs/user/`) |
| Install host | `https://djet.online/jig/install.sh`, `https://djet.online/jig/install.ps1` |
| Index host | `https://djet.online/jig/index/v1/` |
| Libraries / CMake targets | `jig` (executable), static libraries `jig-core`, `jig-index`, `jig-store`, `jig-env`, `jig-build`, `jig-init`, `jig-ui` (alias targets `jig::core`, `jig::index`, ...) |
| Implementation language | C++23, built with CMake and Ninja. Self-hosted with jig from v0.2. |

The name is a workshop term. A jig holds the work in place and guides the tool, so every cut lands where it should. jig does the same for a project: the compiler, the standard and the build are held in place by the project's own files, and every build is guided to the same result.

## GitHub "About" description (350 chars max)

> jig, the native C/C++ toolchain. Pin the compiler, choose the language standard, scaffold a project, and build inside an environment that belongs to that project. One static binary, one manifest, one lockfile. By DJET Systems.

(Length: 226 characters.)

**GitHub license:** `AGPL-3.0-only`.

## Short description (package registries, social cards)

> Pin the compiler. Pick the standard. Build in a project-owned environment.

## Topics

`cpp` `c` `toolchain` `build-tool` `project-manager` `cmake` `ninja` `clang` `llvm` `cli` `cpp23` `reproducible-builds` `developer-tools` `package-manager` `native` `djet-systems`

## Social preview (1280 x 640)

Black field (`#0b0b0c`). DJET mark (white) at left, `jig` set very large in the display face, tagline beneath in the monospace face. Source files: `site/assets/djet-mark-on-black.svg`, `site/assets/djet-lockup-white.svg`.

## Repository layout (target)

```
jig/
  README.md
  REPO.md                      this file
  LICENSE                      AGPL-3.0-only for project-authored material
  THIRD_PARTY_NOTICES.md       vendored dependencies and their licenses
  CHANGELOG.md
  CONTRIBUTING.md
  CODE_OF_CONDUCT.md
  SECURITY.md
  CMakeLists.txt              top level, options, install rules
  CMakePresets.json           dev, ci, release presets (Ninja)
  vcpkg.json                  pinned dependency manifest (planned; today deps are vendored in third_party/)
  third_party/                CLI11, toml++, tl::expected (single headers, licenses beside them)
  cmake/                      toolchain-agnostic helper modules, static-link settings
  src/
    jig/                       main.cpp, thin: parse, dispatch
    jig-core/                  manifest, lock, resolver, errors, config
    jig-index/                 index schema, signature verify, client
    jig-store/                 content-addressed store, download, extract
    jig-env/                   environment creation, shims, activation
    jig-build/                 CMake/Ninja generation, profiles, run/test
    jig-init/                  wizard, templates, std/compiler matrix
    jig-ui/                    output, verbosity, progress, explain
  tools/
    jig-index-builder/         CI tool that generates and signs the index
  templates/                   init templates (exe, lib, header-only; c and c++)
  docs/
    blueprint/                 phased build plan for a coding agent (001 to 012)
    reference/                 CLI, manifest, errors, env vars, platform sheets
    user/                      public documentation
  site/                        landing page (deployed to djet.online/jig)
  tests/                       integration tests and fixtures
  .github/ISSUE_TEMPLATE/      bug report and feature request forms
  .github/PULL_REQUEST_TEMPLATE.md
  .github/workflows/           ci.yml, release.yml, index.yml
```

## Labels (initial set)

`area:cli` `area:index` `area:store` `area:env` `area:build` `area:init` `area:docs` `platform:linux` `platform:macos` `platform:windows` `kind:bug` `kind:feature` `kind:security` `good first issue` `needs-design`

## Release naming

Semantic versioning. Tags `v0.1.0`. Pre-releases `v0.1.0-rc.1`. The index format has its own version (`v1`) and is never changed in place; a new format ships as `v2/` beside `v1/`.

## Voice for public text

Short sentences. State what the tool does and what it does not do. No superlatives, no benchmarks without a reproducible script in the repository. This matches the tone of djet.online: Order, Clarity, Resilience, Discipline, Longevity, Truth.
