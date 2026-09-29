#include "rawpanel.h"

#include <X11/Xatom.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RAWPANEL_PROTOCOL_VERSION 1ul

static int
get_cardinal(Display *dpy, Window win, Atom property, unsigned long *value)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0;
  unsigned char *data = NULL;
  int result = -1;
  if (XGetWindowProperty(dpy, win, property, 0, 1, False, XA_CARDINAL,
                         &type, &format, &count, &after, &data) == Success &&
      type == XA_CARDINAL && format == 32 && count == 1 && after == 0 && data) {
    *value = ((unsigned long *)data)[0]; result = 0;
  }
  if (data) XFree(data);
  return result;
}

static int
supports(Display *dpy, Window owner, Atom property, Atom wanted)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0, i;
  unsigned char *data = NULL;
  int found = 0;
  if (XGetWindowProperty(dpy, owner, property, 0, 64, False, XA_ATOM,
                         &type, &format, &count, &after, &data) != Success ||
      type != XA_ATOM || format != 32 || after != 0) {
    if (data)
      XFree(data);
    return 0;
  }
  for (i = 0; i < count; ++i)
    if (((Atom *)data)[i] == wanted) { found = 1; break; }
  if (data) XFree(data);
  return found;
}

int
rawkeys_rawpanel_init(RawkeysRawpanel *panel, Display *display, int screen)
{
  char name[64];
  if (!panel || !display || screen < 0) return -1;
  memset(panel, 0, sizeof(*panel));
  panel->display = display; panel->screen = screen;
  if (snprintf(name, sizeof(name), "_RAWPANEL_CONTROL_S%d", screen) < 0)
    return -1;
  panel->selection = XInternAtom(display, name, False);
  panel->version = XInternAtom(display, "_RAWPANEL_CONTROL_VERSION", False);
  panel->supported = XInternAtom(display, "_RAWPANEL_SUPPORTED_COMMANDS", False);
  panel->command = XInternAtom(display, "_RAWPANEL_COMMAND", False);
  return 0;
}

int
rawkeys_rawpanel_send(RawkeysRawpanel *panel, const char *action, FILE *err)
{
  struct Pair { const char *action; const char *atom; };
  static const struct Pair pairs[] = {
    {"toggle_visibility", "_RAWPANEL_CMD_TOGGLE_VISIBILITY"},
    {"volume_up", "_RAWPANEL_CMD_VOLUME_UP"},
    {"volume_down", "_RAWPANEL_CMD_VOLUME_DOWN"},
    {"volume_mute", "_RAWPANEL_CMD_VOLUME_MUTE"}
  };
  size_t i;
  Window owner;
  unsigned long version;
  Atom opcode = None;
  XEvent event;
  if (!panel || !action) return -1;
  owner = XGetSelectionOwner(panel->display, panel->selection);
  if (owner == None) {
    if (err) fputs("rawkeys: rawpanel authority is unavailable\n", err);
    return -1;
  }
  if (get_cardinal(panel->display, owner, panel->version, &version) < 0 ||
      version != RAWPANEL_PROTOCOL_VERSION) {
    if (err) fputs("rawkeys: incompatible rawpanel control authority\n", err);
    return -1;
  }
  for (i = 0; i < sizeof(pairs)/sizeof(pairs[0]); ++i)
    if (!strcmp(action, pairs[i].action)) {
      opcode = XInternAtom(panel->display, pairs[i].atom, False); break;
    }
  if (opcode == None || !supports(panel->display, owner, panel->supported, opcode)) {
    if (err) fprintf(err, "rawkeys: rawpanel does not advertise action '%s'\n", action);
    return -1;
  }
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.window = owner;
  event.xclient.message_type = panel->command;
  event.xclient.format = 32;
  event.xclient.data.l[0] = (long)RAWPANEL_PROTOCOL_VERSION;
  event.xclient.data.l[1] = (long)opcode;
  if (!XSendEvent(panel->display, owner, False, NoEventMask, &event)) {
    if (err) fputs("rawkeys: could not send rawpanel command\n", err);
    return -1;
  }
  XFlush(panel->display);
  return 0;
}
