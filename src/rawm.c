#include "rawm.h"

#include <X11/Xatom.h>

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define RAWM_PROTOCOL_VERSION 1ul
#define STATE_LIMIT (1024ul * 1024ul)

static int
get_cardinal(Display *dpy, Window win, Atom property, unsigned long *value)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0;
  unsigned char *data = NULL;
  int ok = -1;

  if (XGetWindowProperty(dpy, win, property, 0, 1, False, XA_CARDINAL,
                         &type, &format, &count, &after, &data) == Success &&
      type == XA_CARDINAL && format == 32 && count == 1 && after == 0 && data) {
    *value = ((unsigned long *)data)[0];
    ok = 0;
  }
  if (data) XFree(data);
  return ok;
}

static int
supported(Display *dpy, Window owner, Atom property, Atom opcode)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0, i;
  unsigned char *data = NULL;
  int found = 0;

  if (XGetWindowProperty(dpy, owner, property, 0, 256, False, XA_ATOM,
                         &type, &format, &count, &after, &data) != Success ||
      type != XA_ATOM || format != 32 || after != 0) {
    if (data) XFree(data);
    return 0;
  }
  for (i = 0; i < count; ++i)
    if (((Atom *)data)[i] == opcode) { found = 1; break; }
  if (data) XFree(data);
  return found;
}

int
rawkeys_rawm_init(RawkeysRawm *rawm, Display *display, int screen)
{
  char name[64];
  if (!rawm || !display || screen < 0) return -1;
  memset(rawm, 0, sizeof(*rawm));
  rawm->display = display;
  rawm->screen = screen;
  if (snprintf(name, sizeof(name), "_RAWM_CONTROL_S%d", screen) < 0)
    return -1;
  rawm->selection = XInternAtom(display, name, False);
  rawm->version = XInternAtom(display, "_RAWM_CONTROL_VERSION", False);
  rawm->supported = XInternAtom(display, "_RAWM_SUPPORTED_COMMANDS", False);
  rawm->state = XInternAtom(display, "_RAWM_STATE", False);
  rawm->command = XInternAtom(display, "_RAWM_COMMAND", False);
  rawm->command_text = XInternAtom(display, "_RAWM_COMMAND_TEXT", False);
  rawm->utf8 = XInternAtom(display, "UTF8_STRING", False);
  return rawkeys_rawm_refresh(rawm, NULL);
}

int
rawkeys_rawm_refresh(RawkeysRawm *rawm, FILE *err)
{
  Window owner;
  unsigned long version;
  if (!rawm || !rawm->display) return -1;
  owner = XGetSelectionOwner(rawm->display, rawm->selection);
  if (owner == None) {
    rawm->owner = None;
    return 0;
  }
  if (get_cardinal(rawm->display, owner, rawm->version, &version) < 0 ||
      version != RAWM_PROTOCOL_VERSION) {
    if (err) fputs("rawkeys: incompatible rawm control authority\n", err);
    rawm->owner = None;
    return -1;
  }
  rawm->owner = owner;
  XSelectInput(rawm->display, owner, PropertyChangeMask | StructureNotifyMask);
  return 1;
}

static int
command_atom(const char *action, char *name, size_t capacity)
{
  struct Pair { const char *action; const char *atom; };
  static const struct Pair pairs[] = {
    {"focus_stack", "_RAWM_CMD_FOCUS_STACK"},
    {"focus_index", "_RAWM_CMD_FOCUS_INDEX"},
    {"inc_master", "_RAWM_CMD_INC_MASTER"},
    {"set_master_factor", "_RAWM_CMD_SET_MASTER_FACTOR"},
    {"zoom", "_RAWM_CMD_ZOOM"}, {"view", "_RAWM_CMD_VIEW"},
    {"toggle_view", "_RAWM_CMD_TOGGLE_VIEW"},
    {"cycle_view", "_RAWM_CMD_CYCLE_VIEW"},
    {"kill_client", "_RAWM_CMD_KILL_CLIENT"},
    {"set_layout", "_RAWM_CMD_SET_LAYOUT"},
    {"cycle_layout", "_RAWM_CMD_CYCLE_LAYOUT"},
    {"toggle_floating", "_RAWM_CMD_TOGGLE_FLOATING"},
    {"tag", "_RAWM_CMD_TAG"}, {"toggle_tag", "_RAWM_CMD_TOGGLE_TAG"},
    {"window_view", "_RAWM_CMD_WINDOW_VIEW"},
    {"toggle_fullscreen", "_RAWM_CMD_TOGGLE_FULLSCREEN"},
    {"focus_monitor", "_RAWM_CMD_FOCUS_MONITOR"},
    {"tag_monitor", "_RAWM_CMD_TAG_MONITOR"},
    {"rename_tag", "_RAWM_CMD_RENAME_TAG"},
    {"quit", "_RAWM_CMD_QUIT"}
  };
  size_t i;
  for (i = 0; i < sizeof(pairs)/sizeof(pairs[0]); ++i) {
    if (!strcmp(action, pairs[i].action)) {
      if (strlen(pairs[i].atom) + 1 > capacity) return -1;
      strcpy(name, pairs[i].atom);
      return 0;
    }
  }
  return -1;
}

static int
send_message(RawkeysRawm *rawm, const char *action,
             long arg0, long arg1, Window requestor, FILE *err)
{
  char atom_name[64];
  Atom opcode;
  XEvent event;

  if (rawkeys_rawm_refresh(rawm, err) <= 0) {
    if (err) fputs("rawkeys: rawm authority is unavailable\n", err);
    return -1;
  }
  if (command_atom(action, atom_name, sizeof(atom_name)) < 0) {
    if (err) fprintf(err, "rawkeys: unknown rawm action '%s'\n", action);
    return -1;
  }
  opcode = XInternAtom(rawm->display, atom_name, False);
  if (!supported(rawm->display, rawm->owner, rawm->supported, opcode)) {
    if (err) fprintf(err, "rawkeys: rawm does not advertise action '%s'\n", action);
    return -1;
  }
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.window = rawm->owner;
  event.xclient.message_type = rawm->command;
  event.xclient.format = 32;
  event.xclient.data.l[0] = (long)RAWM_PROTOCOL_VERSION;
  event.xclient.data.l[1] = (long)opcode;
  event.xclient.data.l[2] = (long)(int32_t)arg0;
  event.xclient.data.l[3] = (long)(int32_t)arg1;
  event.xclient.data.l[4] = (long)requestor;
  if (!XSendEvent(rawm->display, rawm->owner, False, NoEventMask, &event)) {
    if (err) fputs("rawkeys: could not send rawm command\n", err);
    return -1;
  }
  XFlush(rawm->display);
  return 0;
}

int
rawkeys_rawm_send(RawkeysRawm *rawm, const char *action,
                  long arg0, long arg1, FILE *err)
{
  return send_message(rawm, action, arg0, arg1, None, err);
}

static uint64_t
monotonic_ms(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int
rawkeys_rawm_send_text(RawkeysRawm *rawm, const char *action,
                       const char *text, FILE *err)
{
  Window root, requestor;
  XSetWindowAttributes attrs;
  uint64_t deadline;
  int result;

  if (!rawm || !text) return -1;
  if (rawkeys_rawm_refresh(rawm, err) <= 0) return -1;
  root = RootWindow(rawm->display, rawm->screen);
  memset(&attrs, 0, sizeof(attrs));
  attrs.override_redirect = True;
  attrs.event_mask = PropertyChangeMask;
  requestor = XCreateWindow(rawm->display, root, -1, -1, 1, 1, 0, 0,
                            InputOnly, CopyFromParent,
                            CWOverrideRedirect | CWEventMask, &attrs);
  if (requestor == None) return -1;
  XChangeProperty(rawm->display, requestor, rawm->command_text, rawm->utf8, 8,
                  PropModeReplace, (const unsigned char *)text, (int)strlen(text));
  result = send_message(rawm, action, 0, 0, requestor, err);
  if (result < 0) {
    XDestroyWindow(rawm->display, requestor);
    return -1;
  }

  deadline = monotonic_ms() + 2000u;
  for (;;) {
    XEvent event;
    while (XCheckTypedWindowEvent(rawm->display, requestor,
                                  PropertyNotify, &event)) {
      if (event.xproperty.atom == rawm->command_text &&
          event.xproperty.state == PropertyDelete) {
        XDestroyWindow(rawm->display, requestor);
        XFlush(rawm->display);
        return 0;
      }
    }
    {
      uint64_t now = monotonic_ms();
      struct pollfd pfd;
      int timeout;
      int ready;
      if (now >= deadline) break;
      timeout = (int)(deadline - now);
      pfd.fd = ConnectionNumber(rawm->display);
      pfd.events = POLLIN;
      pfd.revents = 0;
      ready = poll(&pfd, 1, timeout);
      if (ready < 0 && errno != EINTR) break;
    }
  }

  if (err)
    fputs("rawkeys: rawm did not consume command text payload\n", err);
  XDestroyWindow(rawm->display, requestor);
  XFlush(rawm->display);
  return -1;
}

static int
read_state(RawkeysRawm *rawm, char **out, FILE *err)
{
  Atom type = None;
  int format = 0;
  unsigned long count = 0, after = 0;
  unsigned char *data = NULL;
  char *copy;
  if (rawkeys_rawm_refresh(rawm, err) <= 0) return -1;
  if (XGetWindowProperty(rawm->display, rawm->owner, rawm->state, 0,
                         (long)(STATE_LIMIT / 4), False, rawm->utf8,
                         &type, &format, &count, &after, &data) != Success ||
      type != rawm->utf8 || format != 8 || after != 0 || !data) {
    if (data) XFree(data);
    return -1;
  }
  copy = malloc((size_t)count + 1u);
  if (!copy) { XFree(data); return -1; }
  memcpy(copy, data, count);
  copy[count] = '\0';
  XFree(data);
  *out = copy;
  return 0;
}

int
rawkeys_rawm_focused(RawkeysRawm *rawm, Window *focused, FILE *err)
{
  char *state = NULL, *save_line = NULL, *line;
  int found = 0;
  if (!focused) return -1;
  *focused = None;
  if (read_state(rawm, &state, err) < 0) return -1;
  line = strtok_r(state, "\n", &save_line);
  if (!line || strcmp(line, "rawm-state-v1")) goto out;
  while ((line = strtok_r(NULL, "\n", &save_line)) != NULL) {
    char *fields[14], *save = NULL, *tok;
    int n = 0;
    if (strncmp(line, "monitor\t", 8)) continue;
    tok = strtok_r(line, "\t", &save);
    while (tok && n < 14) { fields[n++] = tok; tok = strtok_r(NULL, "\t", &save); }
    if (n != 14 || strcmp(fields[0], "monitor")) goto out;
    if (!strcmp(fields[2], "1")) {
      char *end = NULL;
      unsigned long value = strtoul(fields[11], &end, 0);
      if (!end || *end) goto out;
      *focused = (Window)value;
      found = 1;
      break;
    }
  }
out:
  free(state);
  return found ? 0 : -1;
}

bool
rawkeys_rawm_event(const RawkeysRawm *rawm, const XEvent *event)
{
  if (!rawm || !event || rawm->owner == None) return false;
  if (event->type == PropertyNotify)
    return event->xproperty.window == rawm->owner &&
           event->xproperty.atom == rawm->state;
  if (event->type == DestroyNotify)
    return event->xdestroywindow.window == rawm->owner;
  return false;
}
