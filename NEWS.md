# rawkeys NEWS

## 0.1.0

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
