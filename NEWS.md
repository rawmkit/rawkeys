# rawkeys NEWS

## 0.1.0
- Preserve tag-rename capture semantics across the authority split: no output cancels, an empty accepted line restores the numeric tag label, and non-empty input renames the viewed tag.

- Preserve explicit `set_layout` arguments so tile/floating/monocle/bstack/grid bindings select their requested layout instead of degrading to the previous-layout toggle.

- Establish a standalone global-keyboard authority.
- Route window-manager actions through rawm's control membrane.
- Route panel visibility and native volume actions through rawpanel's control
  membrane.
- Execute configured argv vectors without shell reconstruction.
- Preserve per-window XKB group state using rawm-published focus identity.
- Own a per-screen singleton/liveness selection.
- Validate binding targets and action arguments fail-closed.
- Make Xvfb tests parallel-safe and independent of external display probes.
- Treat routing and XKB contract failures as test failures once Xvfb is usable.
