# Versioning and releases

How jig gets numbered, tagged and sealed. The automation is in
`.github/workflows/release.yml`; this sheet is the contract it enforces.

## The scheme

[Semantic Versioning 2.0.0](https://semver.org/): `MAJOR.MINOR.PATCH`.

- While the major is `0` the API (manifest keys, lock format, error codes, event names)
  is explicitly not frozen. Breaking changes bump the **minor**; additions and fixes patch.
- `1.0.0` is the promise: manifest, lock, events and error codes frozen (see roadmap 1.0).
- Pre-releases carry a hyphen: `0.1.0-rc.1`, `0.1.0-rc.2`. They get a prerelease tag on
  GitHub and are never picked by `jig toolchain` resolution unless asked for.

## Tags

Format, exactly:

```
v<MAJOR>.<MINOR>.<PATCH>           stable, e.g. v0.1.0
v<MAJOR>.<MINOR>.<PATCH>-rc.<N>    release candidate
```

Tags are annotated and pushed:

```sh
git tag -a v0.1.0 -m "jig 0.1.0, pin and build"
git push origin v0.1.0
```

On push, the release workflow:

1. Validates the tag (semver 2.0.0, no stray suffixes).
2. Extracts the source tree and stamps it: generated docs book included.
3. Produces a **reproducible tarball** `jig-<version>.tar.gz` — fixed order, fixed
   ownership, mtime taken from the tag commit from `SOURCE_DATE_EPOCH`.
4. Writes the checksum `SHA256SUMS` (`sha256sum jig-<version>.tar.gz`).
5. Publishes a GitHub Release holding the tarball and `SHA256SUMS`, with generated notes.

| Tag | Release on GitHub |
|---|---|
| `v0.1.0` | latest, stable |
| `v0.1.0-rc.3` | marked prerelease |

`v0` tags may be re-tagged only before the rc is published; a published `v*` tag never
moves. A broken patch release is a new tag (`v0.1.1`), never a force push.

## Where the version lives

Single source: `project(jig VERSION <M.m.p>)` in `CMakeLists.txt`. The workflow refuses a
tag whose base does not match that version, so the tree version and the stamp cannot drift
apart.

Inside a build the reported version is:

| How the library was configured | `jig version` prints |
|---|---|
| Plain build | `0.1.0-dev` (suffix marks a development tree) |
| Packager build, with `JIG_RELEASE_VERSION` exported | the exact value given, e.g. `0.1.0` |

Packagers building from the tarball set `JIG_RELEASE_VERSION=<version>` so the binary
answers its release identity. Development builds keep `-dev` on purpose.

## What is in the source tarball

Everything tracked in git at the tag: sources, tests, `docs/`, the generated
`site/docs/` book, `tools/build_site_docs.py`, license and notices. Nothing else.
GitHub also auto-attaches its own `Source code (tar.gz/zip)` snapshots; those are
point-in-time mirrors, not the sealed tarball. Downstreams who verify (packagers,
contribution gateways) use `jig-<version>.tar.gz` plus `SHA256SUMS`.

## Signing

The release key story lives in [RELEASE-0.1.0.md](../RELEASE-0.1.0.md): `SHA256SUMS.sig`
(ed25519) is added at the first tagged release. Until the public key is published,
`SHA256SUMS` alone is the checksum of record.

## Release checklist (per tag)

1. `CHANGELOG.md` updated for the version.
2. `docs/RELEASE-0.1.0.md` (and the matching file for later releases) filled: date,
   hashes, decisions (the draft box must be gone).
3. `project(jig VERSION)` matched (workflow enforces), tests green on the tag commit.
4. Packaging templates updated from the new `SHA256SUMS`: `packaging/aur/PKGBUILD`,
   `packaging/homebrew/jig.rb`, `packaging/deb/debian/changelog`
   ([packaging/README.md](../../packaging/README.md)).
5. Tag, push, workflow runs, tarball + checksum land on the release.
6. Announce: CHANGELOG excerpt to the community channels (site, README stays).