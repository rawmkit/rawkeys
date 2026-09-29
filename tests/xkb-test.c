#include "xkb.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>

static int
ignore_bad_window(Display *display, XErrorEvent *event)
{
  (void)display;
  if (event->error_code == BadWindow)
    return 0;
  return 0;
}

static int
read_group(Display *dpy, Window win, Atom property, unsigned long *group)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0;
  unsigned char *data = NULL;
  int result = -1;

  if (XGetWindowProperty(dpy, win, property, 0, 1, False, XA_CARDINAL,
                         &type, &format, &count, &after, &data) == Success &&
      type == XA_CARDINAL && format == 32 && count == 1 && after == 0 && data) {
    *group = ((unsigned long *)data)[0];
    result = 0;
  }
  if (data) XFree(data);
  return result;
}

int
main(void)
{
  Display *dpy;
  Window root, first, second;
  RawkeysXkb xkb;
  unsigned long group = 99;

  dpy = XOpenDisplay(NULL);
  if (!dpy)
    return 77;
  XSetErrorHandler(ignore_bad_window);
  root = DefaultRootWindow(dpy);
  first = XCreateSimpleWindow(dpy, root, 0, 0, 10, 10, 0, 0, 0);
  second = XCreateSimpleWindow(dpy, root, 20, 0, 10, 10, 0, 0, 0);
  if (first == None || second == None)
    return EXIT_FAILURE;

  if (!XkbLockGroup(dpy, XkbUseCoreKbd, 0))
    return 77;
  if (rawkeys_xkb_init(&xkb, dpy, true, stderr) < 0)
    return 77;
  if (rawkeys_xkb_focus(&xkb, first, stderr) < 0)
    return EXIT_FAILURE;
  XSync(dpy, False);
  if (read_group(dpy, first, xkb.group_atom, &group) < 0 || group != 0)
    return EXIT_FAILURE;

  XDestroyWindow(dpy, first);
  XSync(dpy, False);
  if (rawkeys_xkb_focus(&xkb, second, stderr) < 0)
    return EXIT_FAILURE;
  XSync(dpy, False);
  if (read_group(dpy, second, xkb.group_atom, &group) < 0 || group != 0)
    return EXIT_FAILURE;

  if (rawkeys_xkb_focus(&xkb, None, stderr) < 0)
    return EXIT_FAILURE;
  XDestroyWindow(dpy, second);
  XCloseDisplay(dpy);
  puts("rawkeys xkb: ok");
  return EXIT_SUCCESS;
}
