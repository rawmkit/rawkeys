# rawkeys architecture

rawkeys is the authority for global keyboard policy and per-window XKB group
state. Its plant is the X11 core keyboard. Its observations are admitted key
presses, keyboard-map changes, and rawm-published admitted-client focus identity. Its
actuators are explicit rawm commands, explicit rawpanel commands, and configured
argv execution.

The dependency direction is one-way:

```text
rawkeys ----commands----> rawm
rawkeys ----commands----> rawpanel
rawkeys ----exec-------> external programs
rawm -------state------> rawkeys
```

rawm and rawpanel never depend on rawkeys.

## Truth boundaries

rawkeys does not infer the selected client from `_NET_ACTIVE_WINDOW`, enumerate
clients, interpret rawm private client properties, or mutate rawm state through
EWMH. Focus identity comes only from rawm's authoritative client inventory.

Per-window XKB group state is rawkeys-owned truth. It is stored directly on the
client window as `_RAWKEYS_XKB_GROUP`, so XID destruction also destroys the
associated keyboard state. This avoids a second client-lifetime registry and
XID-reuse ambiguity inside rawkeys.

## Failure domains

If rawm is absent, rawm-target bindings fail closed while exec and rawpanel
bindings remain usable. If rawpanel is absent, panel-target bindings fail closed
without affecting rawm or exec bindings. If rawkeys exits, existing windows,
panel state, and session processes remain valid; only global keyboard policy and
per-window XKB switching disappear.

## Non-authority

rawkeys does not own pointer-driven move/resize, window topology, tag/layout
truth, panel geometry, mixer state, process supervision, application discovery,
clipboard state, secret storage, or menu presentation.
