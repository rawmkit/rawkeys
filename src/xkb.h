#ifndef RAWKEYS_XKB_H
#define RAWKEYS_XKB_H

#include <stdbool.h>
#include <stdio.h>
#include <X11/Xlib.h>

typedef struct {
  Display *display;
  Atom group_atom;
  Window focused;
  bool enabled;
} RawkeysXkb;

int rawkeys_xkb_init(RawkeysXkb *xkb, Display *display, bool enabled, FILE *err);
int rawkeys_xkb_focus(RawkeysXkb *xkb, Window focused, FILE *err);

#endif
