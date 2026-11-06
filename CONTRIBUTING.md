# Contributing to jig

Thank you for helping. jig favours small, reviewed, tested changes.

By submitting a contribution, you agree that it may be distributed under this
project's [AGPL-3.0-only license](LICENSE). You must have the right to submit it.
There is no separate contributor license agreement (CLA). Third-party code must
be identified with its source and license before it is proposed; do not replace
or remove existing third-party notices.

Everyone participating in the project is expected to follow the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Before you start

1. Read `docs/blueprint/001-foundations.md`, especially the decision log. A change that contradicts a decision needs a new decision row first.
2. Open an issue for anything larger than a bug fix. Use the issue templates and label it `needs-design` if the behaviour is not obvious.
3. Check `docs/reference/` for the contract. The sheets win over prose.

## Build and test

```
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Requires a C++23 compiler (Clang 18, GCC 14 or MSVC 19.40 or newer), CMake 3.25+ and Ninja. Dependencies come from `vcpkg.json`.

## Rules

- Library code never prints. It emits events; `jig-ui` renders them.
- Functions that can fail return `std::expected<T, JigError>`. No exceptions cross a library boundary.
- Platform-specific code lives in `jig-core/platform/`.
- A bug fix includes a test that fails without it.
- User-visible text changes update the golden files, and a reviewer reads the diff.
- Format with `clang-format` (the repository `.clang-format`). CI runs `clang-tidy`.
- Error codes `DJxxxx` are stable. Add new ones; never repurpose.
- Keep to the voice of the project: short sentences, say what the tool does and what it does not, no superlatives, no unverified numbers.

## Pull requests

- One topic per pull request. Describe the behaviour change and link the issue.
- Update `docs/user/` and `docs/reference/` in the same pull request.
- Add a line under **Unreleased** in `CHANGELOG.md`.
- Keep changes focused and explain any user-visible or compatibility impact.

## Security issues

Do not open a public issue. Follow the private reporting instructions in `SECURITY.md`.
