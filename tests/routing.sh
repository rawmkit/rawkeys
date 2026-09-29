#!/bin/sh
set -eu

case $0 in */*) test_dir=${0%/*} ;; *) test_dir=. ;; esac
. "$test_dir/support/xvfb.sh"

rawkeys=$1
fake=$2
owner=$3
sendkey=$4
probe=$5
capture=$6
tmp=${TMPDIR:-/tmp}/rawkeys-routing.$$
mkdir -p "$tmp"
auth_pid=
keys_pid=

cleanup()
{
  [ -z "$keys_pid" ] || kill "$keys_pid" 2>/dev/null || true
  [ -z "$keys_pid" ] || wait "$keys_pid" 2>/dev/null || true
  [ -z "$auth_pid" ] || kill "$auth_pid" 2>/dev/null || true
  [ -z "$auth_pid" ] || wait "$auth_pid" 2>/dev/null || true
  test_xvfb_stop
  rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM

skip() { echo "rawkeys routing: skipped ($*)"; exit 77; }
fail() { echo "rawkeys routing failed: $*" >&2; exit 1; }

for tool in Xvfb xdpyinfo; do command -v "$tool" >/dev/null 2>&1 || skip "$tool unavailable"; done
test_xvfb_start "$tmp" || skip "Xvfb unavailable"

cat >"$tmp/rawkeys.conf" <<EOF2
xkb = { per_window = false; };
commands = {
  probe = ("$probe", "$tmp/exec");
  capture = ("$capture");
};
bindings = (
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = 1; },
  { modifiers = (); key = "XF86AudioMute"; target = "rawpanel"; action = "volume_mute"; },
  { modifiers = ("Mod4"); key = "r"; target = "exec"; command = "probe"; },
  { modifiers = ("Mod4"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "capture"; }
);
EOF2

"$fake" "$tmp/authority.log" >"$tmp/fake.out" 2>"$tmp/fake.err" &
auth_pid=$!

n=0
while ! "$owner" _RAWM_CONTROL_S0 >/dev/null 2>&1; do
  n=$((n + 1)); [ "$n" -lt 60 ] || fail "fake rawm authority did not start"; sleep 0.05
done

"$rawkeys" -c "$tmp/rawkeys.conf" >"$tmp/rawkeys.out" 2>"$tmp/rawkeys.err" &
keys_pid=$!
n=0
while ! "$owner" _RAWKEYS_CONTROL_S0 >/dev/null 2>&1; do
  kill -0 "$keys_pid" 2>/dev/null || { cat "$tmp/rawkeys.err" >&2; fail "rawkeys exited"; }
  n=$((n + 1)); [ "$n" -lt 60 ] || fail "rawkeys authority did not become ready"; sleep 0.05
done

wait_log()
{
  needle=$1
  n=0
  while ! grep -F "$needle" "$tmp/authority.log" >/dev/null 2>&1; do
    n=$((n + 1)); [ "$n" -lt 60 ] || { cat "$tmp/authority.log" >&2 || true; fail "missing $needle"; }
    sleep 0.05
  done
}

"$sendkey" Super_L j || rc=$?
[ "${rc:-0}" -ne 77 ] || skip "libXtst unavailable"
[ "${rc:-0}" -eq 0 ] || fail "could not synthesize rawm binding"
wait_log "rawm focus_stack 1 0"

rc=0
"$sendkey" XF86AudioMute || rc=$?
[ "$rc" -ne 77 ] || skip "libXtst unavailable"
[ "$rc" -eq 0 ] || fail "could not synthesize rawpanel binding"
wait_log "rawpanel volume_mute"

rc=0
"$sendkey" Super_L r || rc=$?
[ "$rc" -ne 77 ] || skip "libXtst unavailable"
[ "$rc" -eq 0 ] || fail "could not synthesize exec binding"
n=0
while [ ! -s "$tmp/exec" ]; do
  n=$((n + 1)); [ "$n" -lt 60 ] || fail "exec target did not run"; sleep 0.05
done
[ "$(cat "$tmp/exec")" = exec ] || fail "exec target wrote unexpected result"

rc=0
"$sendkey" Super_L n || rc=$?
[ "$rc" -ne 77 ] || skip "libXtst unavailable"
[ "$rc" -eq 0 ] || fail "could not synthesize rename binding"
wait_log "rawm rename_tag work"

if "$rawkeys" -c "$tmp/rawkeys.conf" >"$tmp/second.out" 2>"$tmp/second.err"; then
  fail "second keyboard authority was admitted"
fi
grep -q 'already running' "$tmp/second.err" ||
  fail "duplicate keyboard authority failure was not explained"

echo "rawkeys routing: ok"
