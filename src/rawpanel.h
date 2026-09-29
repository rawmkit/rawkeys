#ifndef RAWKEYS_RAWPANEL_H
#define RAWKEYS_RAWPANEL_H

#include <stdio.h>
#include <X11/Xlib.h>

typedef struct {
  Display *display;
  int screen;
  Atom selection;
  Atom version;
  Atom supported;
  Atom command;
} RawkeysRawpanel;

int rawkeys_rawpanel_init(RawkeysRawpanel *panel, Display *display, int screen);
int rawkeys_rawpanel_send(RawkeysRawpanel *panel, const char *action, FILE *err);

#endif
