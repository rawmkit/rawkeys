# rawkeys

`rawkeys` is the global keyboard-policy authority for rawmkit.

It owns passive X11 key grabs, maps admitted key chords to explicit actuator
boundaries, and optionally preserves XKB group state per rawm-admitted focused
client. It is not a window manager, panel, launcher, process supervisor, or
shell command language.

A binding has exactly one target:

- `rawm`: send a command through rawm's published control membrane;
- `rawpanel`: send a command through rawpanel's published control membrane;
- `exec`: execute one configured argv vector.

The only composite operation in version 0.1 is rawm tag renaming. A configured
capture command obtains one line of user input and rawkeys forwards that text
through rawm's `rename_tag` command payload. No output cancels the operation; an
empty first line is forwarded as an empty payload and restores rawm's numeric tag
label. This is keyboard-input policy; the window-manager mutation remains rawm
authority.

Per-window keyboard layout state belongs to rawkeys. When enabled, rawkeys
observes focused-client identity from rawm's authoritative client inventory. It
stores the last XKB group in the `_RAWKEYS_XKB_GROUP` property on that client
window and restores it on later focus. It does not infer focus by walking the
X tree or `_NET_ACTIVE_WINDOW`.

## Build

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Dependencies are Xlib and libconfig. Runtime XKB support is provided through
Xlib. The integration suite requires Xvfb. The routing test dynamically loads
`libXtst.so.6` to synthesize real key events; Xtst is a test-runtime dependency
only and is not linked into the rawkeys executable. Once Xvfb starts, routing
and XKB contract failures are hard test failures rather than skips.

## Configuration

System policy is read from `/etc/rawkeys/rawkeys.conf`; user policy overlays it
from `$XDG_CONFIG_HOME/rawkeys/rawkeys.conf` when `XDG_CONFIG_HOME` is absolute,
or from `~/.config/rawkeys/rawkeys.conf` otherwise. Missing default files are
allowed. `-c FILE` selects one explicit file instead.

See `rawkeys.conf(5)`.

## Authority

One rawkeys instance owns `_RAWKEYS_CONTROL_Sn` for each X screen it serves.
The owner window publishes `_RAWKEYS_CONTROL_VERSION = 1`. Version 0.1 exposes
no mutation commands through this selection; it is a singleton/liveness
boundary, not a generic desktop bus.

rawkeys has no authority over pointer-driven move/resize. Those gestures remain
inside rawm because they require admitted client geometry and an active pointer
grab.
