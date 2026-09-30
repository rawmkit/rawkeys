#!/bin/sh
set -eu

rawkeys=$1
default_config=$2
"$rawkeys" -C -c "$default_config" || { echo "rawkeys config test failed: packaged default was rejected" >&2; exit 1; }
tmp=${TMPDIR:-/tmp}/rawkeys-config.$$
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

fail() { echo "rawkeys config test failed: $*" >&2; exit 1; }

cat >"$tmp/valid.conf" <<'EOC'
xkb = { per_window = true; };
commands = {
  terminal = ("xterm");
  tag_name = ("rawmenu", "-M", "input", "-p", "tag");
};
bindings = (
  { modifiers = ("Mod4"); key = "Return"; target = "exec"; command = "terminal"; },
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = 1; },
  { modifiers = ("Mod4"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "tag_name"; },
  { modifiers = (); key = "XF86AudioMute"; target = "rawpanel"; action = "volume_mute"; }
);
EOC
"$rawkeys" -C -c "$tmp/valid.conf" || fail "valid configuration was rejected"

cat >"$tmp/duplicate.conf" <<'EOC'
bindings = (
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = 1; },
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = -1; }
);
EOC
if "$rawkeys" -C -c "$tmp/duplicate.conf" >"$tmp/out" 2>"$tmp/err"; then
  fail "duplicate binding was admitted"
fi
grep -q 'duplicate binding' "$tmp/err" || fail "duplicate binding failure was not explained"

cat >"$tmp/bad-target.conf" <<'EOC'
bindings = (
  { modifiers = (); key = "F1"; target = "desktop"; action = "whatever"; }
);
EOC
if "$rawkeys" -C -c "$tmp/bad-target.conf" >"$tmp/out" 2>"$tmp/err"; then
  fail "unknown target was admitted"
fi
grep -q 'unknown target' "$tmp/err" || fail "unknown target failure was not explained"

cat >"$tmp/bad-rawm.conf" <<'EOC'
bindings = (
  { modifiers = ("Mod4"); key = "j"; target = "rawm"; action = "focus_stack"; number = 2; }
);
EOC
if "$rawkeys" -C -c "$tmp/bad-rawm.conf" >"$tmp/out" 2>"$tmp/err"; then
  fail "invalid rawm arguments were admitted"
fi
grep -q 'invalid arguments' "$tmp/err" || fail "invalid rawm arguments were not explained"

cat >"$tmp/bad-capture.conf" <<'EOC'
bindings = (
  { modifiers = ("Mod4"); key = "n"; target = "rawm"; action = "rename_tag"; capture = "missing"; }
);
EOC
if "$rawkeys" -C -c "$tmp/bad-capture.conf" >"$tmp/out" 2>"$tmp/err"; then
  fail "unknown capture command was admitted"
fi
grep -q 'capture command' "$tmp/err" || fail "capture failure was not explained"

XDG_CONFIG_HOME=relative HOME= "$rawkeys" -C ||
  fail "relative XDG_CONFIG_HOME was not ignored"

echo "rawkeys config: ok"
