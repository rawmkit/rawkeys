# Provenance

`rawkeys` was created as a new repository and implementation on 2026-09-29.
It was designed against the public control contracts of:

- rawm commit `cd7ec90` (`control: publish admitted stack presentation state`);
- rawpanel commit `3036e30` (`docs: use scdoc inline formatting in config manual`).

The integration policy examined while defining the boundary was rawm-config
commit `3a0e826`.

No `src/rawm.c`, in-tree `src/rawmctl`, rawpanel implementation source, or
configuration parser was copied into this repository. Protocol atom names and
wire semantics are implemented independently from the published contracts.
