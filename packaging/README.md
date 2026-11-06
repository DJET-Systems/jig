# Packaging templates

One folder per channel, source of truth for every downstream recipe. Templates
carry the `PASTE_FROM_SHA256SUMS_AT_RELEASE` marker; fill it from the release's
`SHA256SUMS` before publishing that release's package. It is a two-minute, six
files job, done for a reason:

- GoReleaser builds Go projects, cargo-dist builds Rust. jig is C++/CMake: they
  do not apply. There is no generic "universal" automaker for CMake; a short
  handwritten template per ecosystem is the fastest durable path.
- `fpm` covers the compiled-artifact case (`.deb`, `.rpm`, `.arch`, whatever)
  once binary distribution starts: docs/blueprint/010, the heavy bundle, and the
  `apt.djet.online`/Gemfury mirrors.
- When jig 0.2 builds itself with jig, `jig` can own updating these templates
  per release. Until then this folder is updated by hand per tag, and
  `.github/workflows/release.yml` guarantees the source tarball and checksum
  every template needs.

| Folder | Channel | Publish flow |
|---|---|---|
| `aur/` | Arch (AUR, then official after adoption) | see below |
| `homebrew/` | macOS and Linux (tap, then homebrew-core) | see below |
| `deb/debian/` | Debian, Ubuntu (PPA now, Debian when adopted) | see below |
| `snap/` | Snap Store universal (jig 0.2) | `snapcraft push` after `snapcraft` |

## AUR (Arch)

1. Register at aur.archlinux.org and upload a public SSH key.
2. `git clone ssh://aur@aur.archlinux.org/jig.git`
3. Fill `sha256sums` in `PKGBUILD` from the release `SHA256SUMS`, run
   `mksrcinfo` (or regenerate `.SRCINFO` by `makepkg --printsrcinfo > .SRCINFO`).
4. Commit `PKGBUILD` and `.SRCINFO`, `git push`.
5. Test locally first: `makepkg -srci` builds, checks and installs.
6. Official repositories: with votes and a provenance review, a Trusted User may
   adopt `jig` into `[extra]`; the upstream sanitizer (`license`, `source` names)
   above keeps that review short.

## Homebrew (tap)

1. Create the public tap repository `ansgrb/jig-tap` (name must start `homebrew-`).
2. Copy `homebrew/jig.rb` into `Formula/jig.rb` with the release sha256.
3. Users run `brew tap ansgrb/jig https://github.com/ansgrb/jig-tap && brew install jig`.
4. When traction is real, open a PR to Homebrew/homebrew-core; builds must
   succeed on both macOS and Linux runners with their stock toolchains.

## Debian and Ubuntu

Two paths:

- Source package (for Launchpad PPA): the `deb/debian/` skeleton builds from the
  sealed tarball into a native source package, `ppa:djet/jig`, via
  `dput`. `debian/control`, `rules`, `changelog` and `copyright` already carry
  the AGPL-3.0-only details.
- Compiled binary (self-hosted apt orGemfury): build once with `dpkg -b` then push
  the `.deb`; `fpm -s dir -t deb` is the one-command variant when we hand over
  binaries per platform.
