#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void
stop_handler(int sig)
{
  (void)sig;
  running = 0;
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
  Atom atom = XInternAtom(dpy, name, False);
  XChangeProperty(dpy, win, atom, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&value, 1);
}

static void
set_atoms(Display *dpy, Window win, const char *property,
          const char *const *names, size_t count)
{
  Atom *atoms;
  size_t i;
  Atom prop = XInternAtom(dpy, property, False);

  atoms = calloc(count, sizeof(*atoms));
  if (!atoms) exit(EXIT_FAILURE);
  for (i = 0; i < count; ++i)
    atoms[i] = XInternAtom(dpy, names[i], False);
  XChangeProperty(dpy, win, prop, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)atoms, (int)count);
  free(atoms);
}

static int
append_line(const char *path, const char *line)
{
  FILE *fp = fopen(path, "a");
  if (!fp) return -1;
  if (fprintf(fp, "%s\n", line) < 0) {
    fclose(fp);
    return -1;
  }
  return fclose(fp);
}

int
main(int argc, char **argv)
{
  Display *dpy;
  Window root, rawm_owner, panel_owner;
  Atom rawm_sel, panel_sel, rawm_command, panel_command;
  Atom rawm_focus, rawm_layout, rawm_rename, rawm_text, panel_mute;
  Atom utf8, rawm_clients;
  int screen;
  char selection[64];
  struct sigaction sa;
  static const char *const rawm_commands[] = {
    "_RAWM_CMD_FOCUS_STACK", "_RAWM_CMD_SET_LAYOUT",
    "_RAWM_CMD_RENAME_TAG"
  };
  static const char *const panel_commands[] = {
    "_RAWPANEL_CMD_VOLUME_MUTE"
  };
  static const char clients[] = "rawm-clients-v1\n";

  if (argc != 2) {
    fprintf(stderr, "usage: %s LOG\n", argv[0]);
    return EXIT_FAILURE;
  }
  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fputs("fake-authority: cannot open display\n", stderr);
    return EXIT_FAILURE;
  }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  rawm_owner = make_owner(dpy, root);
  panel_owner = make_owner(dpy, root);
  if (rawm_owner == None || panel_owner == None) return EXIT_FAILURE;

  snprintf(selection, sizeof(selection), "_RAWM_CONTROL_S%d", screen);
  rawm_sel = XInternAtom(dpy, selection, False);
  snprintf(selection, sizeof(selection), "_RAWPANEL_CONTROL_S%d", screen);
  panel_sel = XInternAtom(dpy, selection, False);
  XSetSelectionOwner(dpy, rawm_sel, rawm_owner, CurrentTime);
  XSetSelectionOwner(dpy, panel_sel, panel_owner, CurrentTime);

  set_cardinal(dpy, rawm_owner, "_RAWM_CONTROL_VERSION", 1);
  set_atoms(dpy, rawm_owner, "_RAWM_SUPPORTED_COMMANDS",
            rawm_commands, sizeof(rawm_commands) / sizeof(rawm_commands[0]));
  utf8 = XInternAtom(dpy, "UTF8_STRING", False);
  rawm_clients = XInternAtom(dpy, "_RAWM_CLIENTS", False);
  XChangeProperty(dpy, rawm_owner, rawm_clients, utf8, 8, PropModeReplace,
                  (const unsigned char *)clients, (int)strlen(clients));

  set_cardinal(dpy, panel_owner, "_RAWPANEL_CONTROL_VERSION", 1);
  set_atoms(dpy, panel_owner, "_RAWPANEL_SUPPORTED_COMMANDS",
            panel_commands, sizeof(panel_commands) / sizeof(panel_commands[0]));

  rawm_command = XInternAtom(dpy, "_RAWM_COMMAND", False);
  panel_command = XInternAtom(dpy, "_RAWPANEL_COMMAND", False);
  rawm_focus = XInternAtom(dpy, "_RAWM_CMD_FOCUS_STACK", False);
  rawm_layout = XInternAtom(dpy, "_RAWM_CMD_SET_LAYOUT", False);
  rawm_rename = XInternAtom(dpy, "_RAWM_CMD_RENAME_TAG", False);
  rawm_text = XInternAtom(dpy, "_RAWM_COMMAND_TEXT", False);
  panel_mute = XInternAtom(dpy, "_RAWPANEL_CMD_VOLUME_MUTE", False);
  XFlush(dpy);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = stop_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  while (running) {
    while (XPending(dpy)) {
      XEvent ev;
      char line[256];
      XNextEvent(dpy, &ev);
      if (ev.type != ClientMessage) continue;
      if (ev.xclient.window == rawm_owner &&
          ev.xclient.message_type == rawm_command &&
          (Atom)(unsigned long)ev.xclient.data.l[1] == rawm_focus) {
        snprintf(line, sizeof(line), "rawm focus_stack %ld %ld",
                 ev.xclient.data.l[2], ev.xclient.data.l[3]);
        if (append_line(argv[1], line) < 0) return EXIT_FAILURE;
      } else if (ev.xclient.window == rawm_owner &&
                 ev.xclient.message_type == rawm_command &&
                 (Atom)(unsigned long)ev.xclient.data.l[1] == rawm_layout) {
        snprintf(line, sizeof(line), "rawm set_layout %ld %ld",
                 ev.xclient.data.l[2], ev.xclient.data.l[3]);
        if (append_line(argv[1], line) < 0) return EXIT_FAILURE;
      } else if (ev.xclient.window == rawm_owner &&
                 ev.xclient.message_type == rawm_command &&
                 (Atom)(unsigned long)ev.xclient.data.l[1] == rawm_rename) {
        Window requestor = (Window)(unsigned long)ev.xclient.data.l[4];
        Atom type = None;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char *data = NULL;
        if (requestor == None ||
            XGetWindowProperty(dpy, requestor, rawm_text, 0, 1024, True, utf8,
                               &type, &format, &count, &after, &data) != Success ||
            type != utf8 || format != 8 || after != 0 || (count && !data))
          return EXIT_FAILURE;
        if (count)
          snprintf(line, sizeof(line), "rawm rename_tag %.*s", (int)count, data);
        else
          snprintf(line, sizeof(line), "rawm rename_tag ");
        if (data) XFree(data);
        if (append_line(argv[1], line) < 0) return EXIT_FAILURE;
      } else if (ev.xclient.window == panel_owner &&
                 ev.xclient.message_type == panel_command &&
                 (Atom)(unsigned long)ev.xclient.data.l[1] == panel_mute) {
        if (append_line(argv[1], "rawpanel volume_mute") < 0)
          return EXIT_FAILURE;
      }
    }
    {
      struct timespec pause = {0, 10000000L};
      nanosleep(&pause, NULL);
    }
  }

  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
