#include "rawm.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
fail(const char *message)
{
  fprintf(stderr, "rawkeys rawm clients test failed: %s\n", message);
  exit(EXIT_FAILURE);
}

static Window
make_owner(Display *dpy, Window root)
{
  XSetWindowAttributes attrs;

  memset(&attrs, 0, sizeof(attrs));
  attrs.override_redirect = True;
  return XCreateWindow(dpy, root, -1, -1, 1, 1, 0, 0, InputOnly,
                       CopyFromParent, CWOverrideRedirect, &attrs);
}

static void
set_cardinal(Display *dpy, Window win, const char *name, unsigned long value)
{
  Atom prop = XInternAtom(dpy, name, False);

  XChangeProperty(dpy, win, prop, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&value, 1);
}

static void
set_clients(Display *dpy, Window win, const char *value)
{
  Atom prop = XInternAtom(dpy, "_RAWM_CLIENTS", False);
  Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);

  XChangeProperty(dpy, win, prop, utf8, 8, PropModeReplace,
                  (const unsigned char *)value, (int)strlen(value));
  XSync(dpy, False);
}

static void
expect_focused(RawkeysRawm *rawm, Window expected)
{
  Window focused = None;

  if (rawkeys_rawm_focused(rawm, &focused, stderr) < 0)
    fail("valid inventory was rejected");
  if (focused != expected)
    fail("focused client differs");
}

static void
expect_malformed(RawkeysRawm *rawm, const char *inventory)
{
  Window focused = (Window)1;

  set_clients(rawm->display, rawm->owner, inventory);
  if (rawkeys_rawm_focused(rawm, &focused, NULL) == 0 || focused != None)
    fail("malformed inventory was accepted");
}

int
main(void)
{
  Display *dpy = XOpenDisplay(NULL);
  int screen;
  Window root, owner;
  char selection_name[64];
  Atom selection;
  RawkeysRawm rawm;
  static const char valid[] =
    "rawm-clients-v1\n"
    "client\t0x101\t0\t00000001\t1\t1\tAlphaClass\tAlpha%25desk\n"
    "client\t0x202\t0\t00000002\t0\t0\tBetaClass\tBeta desk\n";

  if (!dpy)
    return 77;
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  owner = make_owner(dpy, root);
  if (!owner)
    fail("could not create authority owner");
  snprintf(selection_name, sizeof(selection_name), "_RAWM_CONTROL_S%d", screen);
  selection = XInternAtom(dpy, selection_name, False);
  XSetSelectionOwner(dpy, selection, owner, CurrentTime);
  set_cardinal(dpy, owner, "_RAWM_CONTROL_VERSION", 1);
  set_clients(dpy, owner, valid);

  if (rawkeys_rawm_init(&rawm, dpy, screen) < 0)
    fail("could not discover rawm authority");
  expect_focused(&rawm, (Window)0x101);

  set_clients(dpy, owner, "rawm-clients-v1\n");
  expect_focused(&rawm, None);

  expect_malformed(&rawm,
    "rawm-clients-v1\n"
    "client\t257\t0\t00000001\t1\t1\tAlpha\tAlpha\n");
  expect_malformed(&rawm,
    "rawm-clients-v1\n"
    "client\t0x101\t0\t1\t1\t1\tAlpha\tAlpha\n");
  expect_malformed(&rawm,
    "rawm-clients-v1\n"
    "client\t0x101\t0\t00000001\t0\t1\tAlpha\tAlpha\n");
  expect_malformed(&rawm,
    "rawm-clients-v1\n"
    "client\t0x101\t0\t00000001\t1\t1\tAlpha\tAlpha\n"
    "client\t0x202\t0\t00000001\t1\t1\tBeta\tBeta\n");
  expect_malformed(&rawm,
    "rawm-clients-v2\n");

  XDestroyWindow(dpy, owner);
  XCloseDisplay(dpy);
  puts("rawkeys rawm clients: ok");
  return EXIT_SUCCESS;
}
