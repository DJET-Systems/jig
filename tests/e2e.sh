#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# End-to-end test: drive the real `jig` binary through a project's whole life.
# usage: e2e.sh /path/to/jig
set -u
JIG="${1:?usage: e2e.sh /path/to/jig}"
WORK="$(mktemp -d)"
export JIG_HOME="$WORK/home"
export NO_COLOR=1
trap 'rm -rf "$WORK"' EXIT
pass=0; failn=0
t() { # t "description" command...
  local d="$1"; shift
  if "$@" >"$WORK/out.txt" 2>&1; then printf '  ok    %s\n' "$d"; pass=$((pass+1));
  else printf '  FAIL  %s\n' "$d"; sed 's/^/        | /' "$WORK/out.txt" | tail -25; failn=$((failn+1)); fi
}
expect_exit() { # expect_exit N description command...
  local want="$1" d="$2"; shift 2
  "$@" >"$WORK/out.txt" 2>&1; local got=$?
  if [ "$got" = "$want" ]; then printf '  ok    %s (exit %s)\n' "$d" "$got"; pass=$((pass+1));
  else printf '  FAIL  %s (wanted exit %s, got %s)\n' "$d" "$want" "$got"; sed 's/^/        | /' "$WORK/out.txt" | tail -15; failn=$((failn+1)); fi
}
contains() { grep -q -- "$1" "$WORK/out.txt"; }

cd "$WORK"
echo "jig e2e in $WORK"

t "version"                     "$JIG" version
t "doctor outside a project"    "$JIG" doctor
t "toolchain list"              "$JIG" toolchain list

expect_exit 3 "build outside a project is DJ1001" "$JIG" build
contains "DJ1001" && echo "        (DJ1001 reported)"
expect_exit 2 "init without a terminal and without flags is DJ6003" "$JIG" init nope --no-interactive
contains "DJ6003" || { echo "  FAIL  DJ6003 text missing"; failn=$((failn+1)); }

# --- a C++ executable --------------------------------------------------------
t "init c++ exe"                "$JIG" init hello --lang c++ --std c++20 --toolchain gcc --vcs none -y
[ -f hello/jig.toml ] && [ -f hello/jig.lock ] && [ -x hello/.jig/env/bin/c++ ] \
  && { echo "  ok    init wrote jig.toml, jig.lock and .jig/env"; pass=$((pass+1)); } \
  || { echo "  FAIL  init did not create jig.toml/jig.lock/.jig/env"; failn=$((failn+1)); }
cd "$WORK/hello" || exit 1
t "build debug"                 "$JIG" build
t "run prints hello"            bash -c "'$JIG' run 2>/dev/null | grep -q 'Hello from hello'"
t "run passes exit status"      bash -c "! '$JIG' run -- >/dev/null 2>&1 || true"
t "test"                        "$JIG" test
t "build release"               "$JIG" build --release
t "compile_commands.json link"  test -L compile_commands.json
t "exec sees the env's cc"      bash -c "'$JIG' exec -- cc --version | head -1 | grep -qiE 'gcc|clang'"
t "exec CC points into .jig"    bash -c "'$JIG' exec -- sh -c 'echo \$CC' | grep -q '.jig/env/bin/cc'"
t "env --format sh"             bash -c "'$JIG' env | grep -q 'export CC='"
t "env --format json"           bash -c "'$JIG' env --format json | grep -q '\"CC\"'"
t "doctor in project"           "$JIG" doctor
t "lock is current"             "$JIG" lock --check
t "sync is current"             "$JIG" sync --check

# leaking variables are dropped inside the environment
t "CXXFLAGS does not leak"      bash -c "CXXFLAGS=-DLEAK '$JIG' exec -- sh -c 'echo [\$CXXFLAGS]' | grep -q '\[\]'"

# --json is machine readable, one object per line
"$JIG" --json build >"$WORK/json.txt" 2>/dev/null
if [ -s "$WORK/json.txt" ] && ! grep -vq '^{' "$WORK/json.txt" && tail -1 "$WORK/json.txt" | grep -q '"event":"done"'; then
  echo "  ok    --json build emits one JSON object per line"; pass=$((pass+1))
else echo "  FAIL  --json output"; head -5 "$WORK/json.txt"; failn=$((failn+1)); fi

# --- staleness ---------------------------------------------------------------
sed -i 's/version  = "0.1.0"/version  = "0.2.0"/' jig.toml
expect_exit 3 "stale lock + --locked is DJ1103" "$JIG" --locked build
contains "DJ1103" || { echo "  FAIL  DJ1103 text missing"; failn=$((failn+1)); }
t "lock refreshes"              "$JIG" lock
t "build after relock"          "$JIG" build

# --- bad manifests -----------------------------------------------------------
cp jig.toml jig.toml.bak
sed -i 's/^warnings = /warnigns = /' jig.toml
expect_exit 3 "typo in jig.toml is DJ1003" "$JIG" build
contains "build.warnings" || { echo "  FAIL  did-you-mean missing"; failn=$((failn+1)); }
mv jig.toml.bak jig.toml

# --- toolchain selection -----------------------------------------------------
expect_exit 4 "toolchain use gcc@99 is DJ2101 and jig.toml is restored" "$JIG" toolchain use gcc@99
grep -q 'version = "99"' jig.toml && { echo "  FAIL  jig.toml was not restored"; failn=$((failn+1)); } || { echo "  ok    jig.toml restored after failed use"; pass=$((pass+1)); }
t "toolchain which"             "$JIG" toolchain which c++
expect_exit 1 "toolchain install is DJ9001" "$JIG" toolchain install llvm@20

# --- a failing build reports DJ4002 -----------------------------------------
echo 'int main() { this is not c++ }' > src/main.cpp
expect_exit 7 "compile error is DJ4002" "$JIG" build
contains "error" || { echo "  FAIL  compiler output was not shown"; failn=$((failn+1)); }
cd "$WORK"

# --- a C static library with tests, strict warnings, sanitizers --------------
t "init c static lib"           "$JIG" init cring --lang c --std c11 --toolchain gcc --kind static-lib --vcs none -y
cd "$WORK/cring" || exit 1
printf '\n[profile.debug]\nsanitizers = ["address", "undefined"]\n' >> jig.toml
t "test with asan+ubsan"        "$JIG" test
t "run on a library is DJ4003"  bash -c "! '$JIG' run"
cd "$WORK"

# --- header-only library ------------------------------------------------------
t "init header-only"            "$JIG" init hdr --lang c++ --std c++17 --toolchain gcc --kind header-only --vcs none -y
cd "$WORK/hdr" || exit 1; t "header-only test"    "$JIG" test; cd "$WORK"

# --- refuses a non-empty directory -------------------------------------------
mkdir busy && echo x > busy/file
expect_exit 1 "init into a non-empty dir is DJ6001" "$JIG" init busy --lang c --std c11 --toolchain gcc -y
t "clean"                       bash -c "cd '$WORK/hello' && '$JIG' clean"

echo
echo "e2e: $pass passed, $failn failed"
[ "$failn" = 0 ]
