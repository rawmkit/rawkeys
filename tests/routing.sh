#!/bin/sh
set -eu

case $0 in */*) test_dir=${0%/*} ;; *) test_dir=. ;; esac
. "$test_dir/support/xvfb.sh"

rawkeys=$1
display_probe=$2
fake=$3
owner=$4
sendkey=$5
probe=$6
capture=$7
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

command -v Xvfb >/dev/null 2>&1 || skip "Xvfb unavailable"
test_xvfb_start "$tmp" "$display_probe" || skip "Xvfb unusable"

cat >"$tmp/rawkeys.conf" <<EOF2
xkb = { per_window = false; };
commands = {
  probe = ("$probe", "$tmp/exec");
  capture = ("$capture", "value");
  capture_empty = ("$capture", "empty");
  capture_cancel = ("$capture", "cancel");
};
bindings = (
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = 1; },
  { modifiers = ("Mod4"); key = "t"; target = "rawm"; action = "set_layout"; layout = "tile"; },
  { modifiers = (); key = "XF86AudioMute"; target = "rawpanel"; action = "volume_mute"; },
  { modifiers = ("Mod4"); key = "r"; target = "exec"; command = "probe"; },
  { modifiers = ("Mod4"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "capture"; },
  { modifiers = ("Mod4", "Shift"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "capture_empty"; },
  { modifiers = ("Mod4", "Control"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "capture_cancel"; }
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
[ "${rc:-0}" -eq 0 ] || fail "could not synthesize rawm binding"
wait_log "rawm focus_stack 1 0"

rc=0
"$sendkey" Super_L t || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize explicit layout binding"
wait_log "rawm set_layout 0 0"

rc=0
"$sendkey" XF86AudioMute || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize rawpanel binding"
wait_log "rawpanel volume_mute"

rc=0
"$sendkey" Super_L r || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize exec binding"
n=0
while [ ! -s "$tmp/exec" ]; do
  n=$((n + 1)); [ "$n" -lt 60 ] || fail "exec target did not run"; sleep 0.05
done
[ "$(cat "$tmp/exec")" = exec ] || fail "exec target wrote unexpected result"

rc=0
"$sendkey" Super_L n || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize rename binding"
wait_log "rawm rename_tag work"

rc=0
"$sendkey" Super_L Shift_L n || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize empty rename binding"
n=0
while [ "$(grep -Fxc "rawm rename_tag " "$tmp/authority.log" 2>/dev/null || true)" -lt 1 ]; do
  n=$((n + 1))
  [ "$n" -lt 60 ] || { cat "$tmp/authority.log" >&2 || true; fail "missing exact empty rename record"; }
  sleep 0.05
done
empty_count=$(grep -Fxc "rawm rename_tag " "$tmp/authority.log" || true)
[ "$empty_count" -eq 1 ] || fail "empty captured line was not forwarded exactly once"

rc=0
"$sendkey" Super_L Control_L n || rc=$?
[ "$rc" -eq 0 ] || fail "could not synthesize cancelled rename binding"
sleep 0.15
[ "$(grep -c '^rawm rename_tag ' "$tmp/authority.log" || true)" -eq 2 ] ||
  fail "capture cancellation emitted a rename command"

if "$rawkeys" -c "$tmp/rawkeys.conf" >"$tmp/second.out" 2>"$tmp/second.err"; then
  fail "second keyboard authority was admitted"
fi
grep -q 'already running' "$tmp/second.err" ||
  fail "duplicate keyboard authority failure was not explained"

echo "rawkeys routing: ok"
