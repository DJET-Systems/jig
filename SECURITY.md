# Security policy

jig downloads and runs programs on your machine, so its trust model is part of the product. This file states it plainly.

This repository is pre-release. Sections below describe design targets where noted;
they are not a claim that unreleased functionality or release infrastructure is
already available. Check [CODE-STATUS.md](CODE-STATUS.md) before relying on a
feature.

## Reporting a vulnerability

Use **Report a vulnerability** in the repository's GitHub **Security** tab to
send a private report. Repository maintainers must enable GitHub private
vulnerability reporting in the repository settings for this channel to be
available. Do not include vulnerability details in public issues or discussions.

There is no response-time guarantee. Maintainers will acknowledge reports and
coordinate disclosure and fixes privately. Fixes will be noted in
`CHANGELOG.md`; advisories will be published when appropriate.

## What jig trusts

| Item | How it is protected |
|---|---|
| The toolchain index | Signed with ed25519. Verified before use. No unsigned fallback and no flag to skip verification. |
| Toolchain archives | Size and sha256 come from the signed index and are checked after download. Extraction rejects path traversal, escaping links and archive bombs. Nothing from an archive is executed during install. |
| jig itself | Release archives have a `SHA256SUMS` file signed with the release key. `jig self verify` and `jig self update` check it. The first install through the one-line installer relies on TLS to `djet.online`; verify by hand if that is not enough for you. |
| Your project | `jig.toml` is configuration. jig never runs code from it. `[env] set` values are never logged and are redacted in reports. |

## What jig does not do

- No telemetry in 0.1. Any future telemetry will be opt-in.
- It does not upload reports. `jig doctor --report` writes a local file for you to read and attach.
- It does not install system packages, run `sudo`, or change anything outside your user directory and the project.

## Release keys

Release signing and index keys are not yet operational for a public release.
Do not treat release artifacts as verified until the public keys and their
verification instructions have been published. Before release, the owner must
document and test key custody, rotation, revocation and recovery.

| Key | Purpose | Required handling |
|---|---|---|
| Root | Signs `keys.json`; public half compiled into jig | Keep signing material offline and access-controlled. |
| Index | Signs `index.json` | Store signing material separately from public artifacts and support rotation. |
| Release | Signs `SHA256SUMS` | Restrict signing access and publish verification instructions. |

The planned public-key location is `https://djet.online/jig/keys/`; until it is
live, do not assume a key is published there. The index design is documented in
`docs/blueprint/004-index-and-store.md`.

## Supported versions

The latest release receives security fixes. Before 1.0 there are no long-term support branches.
