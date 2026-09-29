#include "xkb.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>

#include <string.h>

static int
current_group(Display *display, unsigned int *group)
{
  XkbStateRec state;
  if (XkbGetState(display, XkbUseCoreKbd, &state) != Success) return -1;
  *group = state.group;
  return 0;
}

static void
store_group(RawkeysXkb *xkb, Window window, unsigned int group)
{
  unsigned long value = group;
  if (window == None) return;
  XChangeProperty(xkb->display, window, xkb->group_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&value, 1);
}

static int
load_group(RawkeysXkb *xkb, Window window, unsigned int *group)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0;
  unsigned char *data = NULL;
  int result = -1;
  if (window == None) return -1;
  if (XGetWindowProperty(xkb->display, window, xkb->group_atom, 0, 1, False,
                         XA_CARDINAL, &type, &format, &count, &after, &data) == Success &&
      type == XA_CARDINAL && format == 32 && count == 1 && after == 0 && data &&
      ((unsigned long *)data)[0] <= 3ul) {
    *group = (unsigned int)((unsigned long *)data)[0];
    result = 0;
  }
  if (data) XFree(data);
  return result;
}

int
rawkeys_xkb_init(RawkeysXkb *xkb, Display *display, bool enabled, FILE *err)
{
  int opcode, event, error, major = XkbMajorVersion, minor = XkbMinorVersion;
  if (!xkb || !display) return -1;
  memset(xkb, 0, sizeof(*xkb));
  xkb->display = display;
  xkb->enabled = enabled;
  xkb->group_atom = XInternAtom(display, "_RAWKEYS_XKB_GROUP", False);
  if (!enabled) return 0;
  if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
    if (err) fputs("rawkeys: XKB extension is unavailable\n", err);
    return -1;
  }
  return 0;
}

int
rawkeys_xkb_focus(RawkeysXkb *xkb, Window focused, FILE *err)
{
  unsigned int group;
  if (!xkb || !xkb->enabled || focused == xkb->focused) return 0;

  if (xkb->focused != None && current_group(xkb->display, &group) == 0)
    store_group(xkb, xkb->focused, group);

  xkb->focused = focused;
  if (focused == None) return 0;

  if (load_group(xkb, focused, &group) < 0) {
    if (current_group(xkb->display, &group) < 0) {
      if (err) fputs("rawkeys: cannot read XKB keyboard group\n", err);
      return -1;
    }
    store_group(xkb, focused, group);
  }
  if (!XkbLockGroup(xkb->display, XkbUseCoreKbd, group)) {
    if (err) fprintf(err, "rawkeys: cannot restore XKB group %u\n", group);
    return -1;
  }
  XFlush(xkb->display);
  return 0;
}
