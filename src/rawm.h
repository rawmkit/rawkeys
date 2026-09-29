#ifndef RAWKEYS_RAWM_H
#define RAWKEYS_RAWM_H

#include <stdbool.h>
#include <stdio.h>
#include <X11/Xlib.h>

typedef struct {
  Display *display;
  int screen;
  Atom selection;
  Atom version;
  Atom supported;
  Atom state;
  Atom command;
  Atom command_text;
  Atom utf8;
  Window owner;
} RawkeysRawm;

int rawkeys_rawm_init(RawkeysRawm *rawm, Display *display, int screen);
int rawkeys_rawm_refresh(RawkeysRawm *rawm, FILE *err);
int rawkeys_rawm_send(RawkeysRawm *rawm, const char *action,
                      long arg0, long arg1, FILE *err);
int rawkeys_rawm_send_text(RawkeysRawm *rawm, const char *action,
                           const char *text, FILE *err);
int rawkeys_rawm_focused(RawkeysRawm *rawm, Window *focused, FILE *err);
bool rawkeys_rawm_event(const RawkeysRawm *rawm, const XEvent *event);

#endif
