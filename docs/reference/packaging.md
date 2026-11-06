# Packaging and distribution

How jig reaches distributions beyond the release archive. The release archive is the source
of truth; every package below points at it and re-verifies it. Nothing in the store is the
upstream: always install through the pinned hash if you care about provenance.

## Status

| Channel | When | Owner of the recipe |
|---|---|---|
| GitHub Releases + `install.sh` / `install.ps1` | 0.1 | jig (official) |
| AUR (`jig`, `jig-bin`) | 0.1 | community, blessed recipe kept here |
| PPA (`ppa:djet/jig`, Ubuntu) | 0.1–0.2 | jig |
| COPR (`djet/jig`, Fedora) | 0.1–0.2 | jig |
| Snap (classic confinement) | 0.2 | jig |
| Gemfury (turnkey apt/rpm repo) | fallback, only if self-hosting the apt repo slips | jig |
| Debian, Fedora, homebrew-core, winget official | after adoption | the distributions |

## Layering

```
signed release archive      ← provenance ends here (sha256, SHA256SUMS.sig, SBOM)
  ├─ GitHub Releases        the artifact
  ├─ AUR / COPR / PPA       build from source or repackage the artifact
  ├─ Snap                   classic fixture around the archive
  └─ Gemfury                mirrors the artifacts for apt/yum users
```

Rules every channel must satisfy:

1. The package carries the exact archive hash in its recipe or changelog. A package that
   cannot name its hash is rejected.
2. jig keeps pinning its own compilers; the distribution's clang is only used to build jig
   itself and never enters a build jig performs.
3. `jig self update` refuses to run inside a store-managed install (snap, distro package)
   and tells the user to update through that channel instead.

## Ubuntu, PPA

Launchpad builds the deb for supported LTS releases. Recipe lives in `packaging/deb/`.

```sh
sudo add-apt-repository ppa:djet/jig
sudo apt install jig
```

Static binary, so the deb only ships the archive contents plus manpage and completions.
`apt.djet.online` (self-hosted, signed, one command to add) replaces the PPA when DNS and
signing keys are ready; until then the PPA carries it.

## Fedora, COPR

```sh
sudo dnf copr enable djet/jig
sudo dnf install jig
```

Same static rule as the deb. The spec live in `packaging/rpm/jig.spec`.

## Arch, AUR

Two families, both upstream:

- `jig-bin` — wraps the Linux x86_64 release archive, checks the sha256 against the
  archive. Preferred for most users.
- `jig` — builds the current tag from source (CMake + Ninja + a C++23 compiler).

AUR is maintained in the repo under `packaging/aur/` and pushed to the AUR for each tag.

## Snap (0.2)

`snapcraft.yaml` defines the build rules and publishes straight to the Snap Store.

```yaml
grade: stable
confinement: classic        # a toolchain must see the user's toolchain paths
apps:
  jig:
    command: bin/jig
```

Classic confinement because `jig shell`, `jig exec` and the project environment manipulate
paths across the whole system; strict confinement would break every command that runs a
compiler. Candidate for the store after the classic review.

Flatpak was considered and cut: sandboxed GUI-style packaging is wrong for a compiler
driver, and non-classified Flatpak will not give jig the paths it needs either.

## Gemfury

If the self-hosted apt repo is late and the PPA is not enough, Gemfury serves signed
artifacts over apt/yum without operating infrastructure. Plan: instructions appear on the
Install section and this sheet; the recipe is a script to push the deb/rpm to the account.

## Heavy release

The heavy artifact is a v0.1-plus bundle for one platform, everything pinned by the signed
index included: the archive of every indexed compiler version that tier-1 platforms can
use, plus CMake, Ninja, and the toolchain's own record. The artifacts are in the extra
release section and get registered in `docs/RELEASE-0.1.0.md`'s checksums.

| Artifact (per platform) | Contents | Size class |
|---|---|---|
| `jig-<v>-<host>-heavy.tar.xz` | jig + all indexed clang versions + CMake + Ninja | multiple GiB |
| `jig-<v>-<host>.tar.xz` | jig alone | ~10 MiB |

`jig doctor --release-heavy` (0.1.x) validates the heavy unpack: every archive revision
expected by the index exists, every hash is correct, and the report ends in H1 on a machine
with no network. Traditional install paths stay default: heavy is for CI, airgapped and
build farm machines that cannot download at build time.

Design sheets about the index itself (what the heavy tar is built from) stay in
`docs/blueprint/004`.
