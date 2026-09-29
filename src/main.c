#include "config.h"
#include "rawm.h"
#include "rawpanel.h"
#include "xkb.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "unknown"
#endif

#define CAPTURE_LIMIT 4096u
#define LENGTH(a) (sizeof(a) / sizeof((a)[0]))

static Display *dpy;
static Window root;
static int screen_no;
static unsigned int numlockmask;
static int signalpipe[2] = {-1, -1};
static volatile sig_atomic_t signal_write_fd = -1;
static int grab_error;
static Window authority_window;
static Atom authority_selection;


static int
authority_init(void)
{
  char name[64];
  XSetWindowAttributes attrs;
  Atom version_atom;
  unsigned long version = 1;

  if (snprintf(name, sizeof(name), "_RAWKEYS_CONTROL_S%d", screen_no) < 0 ||
      strlen(name) >= sizeof(name))
    return -1;
  authority_selection = XInternAtom(dpy, name, False);
  version_atom = XInternAtom(dpy, "_RAWKEYS_CONTROL_VERSION", False);
  if (XGetSelectionOwner(dpy, authority_selection) != None) {
    fputs("rawkeys: keyboard authority is already running on this screen\n", stderr);
    return -1;
  }
  memset(&attrs, 0, sizeof(attrs));
  attrs.override_redirect = True;
  authority_window = XCreateWindow(dpy, root, -1, -1, 1, 1, 0, 0,
                                   InputOnly, CopyFromParent,
                                   CWOverrideRedirect, &attrs);
  if (authority_window == None)
    return -1;
  XSetSelectionOwner(dpy, authority_selection, authority_window, CurrentTime);
  XSync(dpy, False);
  if (XGetSelectionOwner(dpy, authority_selection) != authority_window)
    return -1;
  XChangeProperty(dpy, authority_window, version_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&version, 1);
  XFlush(dpy);
  return 0;
}

static unsigned int
runtime_modifiers(unsigned int modifiers)
{
  unsigned int mask = 0;
  if (modifiers & RAWKEYS_MOD_SHIFT) mask |= ShiftMask;
  if (modifiers & RAWKEYS_MOD_CONTROL) mask |= ControlMask;
  if (modifiers & RAWKEYS_MOD_1) mask |= Mod1Mask;
  if (modifiers & RAWKEYS_MOD_2) mask |= Mod2Mask;
  if (modifiers & RAWKEYS_MOD_3) mask |= Mod3Mask;
  if (modifiers & RAWKEYS_MOD_4) mask |= Mod4Mask;
  if (modifiers & RAWKEYS_MOD_5) mask |= Mod5Mask;
  return mask;
}

static void
updatenumlockmask(void)
{
  XModifierKeymap *map;
  KeyCode numlock;
  int i, j;
  numlockmask = 0;
  numlock = XKeysymToKeycode(dpy, XK_Num_Lock);
  map = XGetModifierMapping(dpy);
  if (!map) return;
  for (i = 0; i < 8; ++i)
    for (j = 0; j < map->max_keypermod; ++j)
      if (map->modifiermap[i * map->max_keypermod + j] == numlock)
        numlockmask = (1u << i);
  XFreeModifiermap(map);
}

static unsigned int
cleanmask(unsigned int mask)
{
  return mask & ~(numlockmask | LockMask) &
         (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask);
}

static int
xerror(Display *display, XErrorEvent *event)
{
  char text[128];
  (void)display;
  if (event->error_code == BadWindow) return 0;
  if (event->error_code == BadAccess && grab_error >= 0) {
    grab_error = 1;
    return 0;
  }
  XGetErrorText(dpy, event->error_code, text, sizeof(text));
  fprintf(stderr, "rawkeys: X error: %s (request %u.%u)\n",
          text, event->request_code, event->minor_code);
  return 0;
}

static int
grabkeys(const RawkeysConfig *cfg)
{
  static const unsigned int extras[] = {0, LockMask, 0, 0};
  size_t i, j;
  unsigned int masks[4];
  updatenumlockmask();
  masks[0] = extras[0];
  masks[1] = extras[1];
  masks[2] = numlockmask;
  masks[3] = numlockmask | LockMask;
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  grab_error = 0;
  for (i = 0; i < cfg->binding_count; ++i) {
    KeySym sym = XStringToKeysym(cfg->bindings[i].key);
    KeyCode code;
    unsigned int mod;
    if (sym == NoSymbol) {
      fprintf(stderr, "rawkeys: unknown keysym '%s'\n", cfg->bindings[i].key);
      return -1;
    }
    code = XKeysymToKeycode(dpy, sym);
    if (!code) {
      fprintf(stderr, "rawkeys: no keycode for '%s'\n", cfg->bindings[i].key);
      return -1;
    }
    mod = runtime_modifiers(cfg->bindings[i].modifiers);
    for (j = 0; j < LENGTH(masks); ++j)
      XGrabKey(dpy, code, mod | masks[j], root, True,
               GrabModeAsync, GrabModeAsync);
  }
  XSync(dpy, False);
  if (grab_error) {
    fputs("rawkeys: one or more global key grabs are already owned\n", stderr);
    return -1;
  }
  grab_error = -1;
  return 0;
}

static const RawkeysBinding *
find_binding(const RawkeysConfig *cfg, XKeyEvent *event)
{
  KeySym sym = XkbKeycodeToKeysym(dpy, (KeyCode)event->keycode, 0, 0);
  size_t i;
  for (i = 0; i < cfg->binding_count; ++i) {
    KeySym wanted = XStringToKeysym(cfg->bindings[i].key);
    if (wanted == sym &&
        cleanmask(runtime_modifiers(cfg->bindings[i].modifiers)) == cleanmask(event->state))
      return &cfg->bindings[i];
  }
  return NULL;
}

static int
spawn_command(const RawkeysCommand *cmd)
{
  pid_t pid;
  if (!cmd || !cmd->argv || !cmd->argv[0]) return -1;
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "rawkeys: fork: %s\n", strerror(errno));
    return -1;
  }
  if (pid == 0) {
    if (dpy) close(ConnectionNumber(dpy));
    (void)setsid();
    execvp(cmd->argv[0], cmd->argv);
    fprintf(stderr, "rawkeys: execvp %s: %s\n", cmd->argv[0], strerror(errno));
    _exit(127);
  }
  return 0;
}

static int
capture_command(const RawkeysCommand *cmd, char *buffer, size_t capacity)
{
  int fds[2], status;
  pid_t pid;
  size_t used = 0;
  int got_line = 0;
  int overflow = 0;
  if (!cmd || !cmd->argv || !cmd->argv[0] || !buffer || capacity < 2) return -1;
  if (pipe(fds) < 0) {
    fprintf(stderr, "rawkeys: capture pipe: %s\n", strerror(errno));
    return -1;
  }
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "rawkeys: capture fork: %s\n", strerror(errno));
    close(fds[0]); close(fds[1]); return -1;
  }
  if (pid == 0) {
    int nullfd;
    close(fds[0]);
    nullfd = open("/dev/null", O_RDONLY);
    if (nullfd < 0 || dup2(nullfd, STDIN_FILENO) < 0 ||
        dup2(fds[1], STDOUT_FILENO) < 0)
      _exit(127);
    if (nullfd != STDIN_FILENO) close(nullfd);
    if (fds[1] != STDOUT_FILENO) close(fds[1]);
    if (dpy) close(ConnectionNumber(dpy));
    (void)setsid();
    execvp(cmd->argv[0], cmd->argv);
    _exit(127);
  }
  close(fds[1]);
  buffer[0] = '\0';
  while (!got_line) {
    char chunk[128];
    ssize_t n = read(fds[0], chunk, sizeof(chunk));
    ssize_t i;
    if (n < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (n == 0) break;
    for (i = 0; i < n; ++i) {
      if (chunk[i] == '\n') { got_line = 1; break; }
      if (chunk[i] == '\r') continue;
      if (used + 1u < capacity)
        buffer[used++] = chunk[i];
      else
        overflow = 1;
    }
  }
  close(fds[0]);
  buffer[used] = '\0';
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  if (overflow) {
    fputs("rawkeys: captured command output exceeds the supported line limit\n", stderr);
    return -1;
  }
  if (!used) return 0;
  return 1;
}

static int
layout_id(const char *layout)
{
  static const char *const names[] = {
    "tile", "floating", "monocle", "bstack", "bstack_horizontal", "grid"
  };
  size_t i;
  if (!layout) return -1;
  for (i = 0; i < LENGTH(names); ++i)
    if (!strcmp(layout, names[i])) return (int)i;
  return -2;
}

static int
rawm_args(const RawkeysBinding *b, long *arg0, long *arg1)
{
  *arg0 = 0; *arg1 = 0;
  if (!strcmp(b->action, "focus_stack") || !strcmp(b->action, "focus_index") ||
      !strcmp(b->action, "inc_master") || !strcmp(b->action, "cycle_view") ||
      !strcmp(b->action, "cycle_layout") || !strcmp(b->action, "focus_monitor") ||
      !strcmp(b->action, "tag_monitor")) {
    if (!b->has_number) return -1;
    *arg0 = b->number;
  } else if (!strcmp(b->action, "set_master_factor")) {
    if (b->has_factor) { *arg0 = (long)(b->factor * 1000000.0); *arg1 = 1; }
    else if (b->has_delta) { *arg0 = (long)(b->delta * 1000000.0); *arg1 = 0; }
    else return -1;
  } else if (!strcmp(b->action, "view")) {
    if (b->has_tag) *arg0 = b->all_tags ? -1 : (long)b->tag;
    else *arg0 = 0;
  } else if (!strcmp(b->action, "toggle_view") || !strcmp(b->action, "tag") ||
             !strcmp(b->action, "toggle_tag")) {
    if (!b->has_tag) return -1;
    *arg0 = b->all_tags ? -1 : (long)b->tag;
  } else if (!strcmp(b->action, "set_layout")) {
    int id = b->has_layout ? layout_id(b->layout) : -1;
    if (id < -1) return -1;
    *arg0 = id;
  } else if (!strcmp(b->action, "quit")) {
    *arg0 = b->restart ? 1 : 0;
  }
  return 0;
}

static int
dispatch_binding(const RawkeysConfig *cfg, const RawkeysBinding *b,
                 RawkeysRawm *rawm, RawkeysRawpanel *panel)
{
  if (b->target == RAWKEYS_TARGET_EXEC) {
    return spawn_command(rawkeys_config_find_command(cfg, b->command));
  }
  if (b->target == RAWKEYS_TARGET_RAWPANEL)
    return rawkeys_rawpanel_send(panel, b->action, stderr);
  if (b->target == RAWKEYS_TARGET_RAWM) {
    long arg0, arg1;
    if (!strcmp(b->action, "rename_tag")) {
      char text[CAPTURE_LIMIT];
      const RawkeysCommand *cmd = rawkeys_config_find_command(cfg, b->capture);
      int captured = capture_command(cmd, text, sizeof(text));
      if (captured <= 0) return captured < 0 ? -1 : 0;
      return rawkeys_rawm_send_text(rawm, b->action, text, stderr);
    }
    if (rawm_args(b, &arg0, &arg1) < 0) {
      fprintf(stderr, "rawkeys: invalid arguments for rawm action '%s'\n", b->action);
      return -1;
    }
    return rawkeys_rawm_send(rawm, b->action, arg0, arg1, stderr);
  }
  return -1;
}

static void
signal_handler(int sig)
{
  unsigned char byte = (unsigned char)sig;
  int fd = (int)signal_write_fd;
  if (fd >= 0) (void)write(fd, &byte, 1);
}

static int
install_signals(void)
{
  struct sigaction sa;
  if (pipe(signalpipe) < 0) return -1;
  fcntl(signalpipe[0], F_SETFL, fcntl(signalpipe[0], F_GETFL) | O_NONBLOCK);
  fcntl(signalpipe[1], F_SETFL, fcntl(signalpipe[1], F_GETFL) | O_NONBLOCK);
  fcntl(signalpipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(signalpipe[1], F_SETFD, FD_CLOEXEC);
  signal_write_fd = signalpipe[1];
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGTERM, &sa, NULL) < 0 || sigaction(SIGINT, &sa, NULL) < 0 ||
      sigaction(SIGHUP, &sa, NULL) < 0 || sigaction(SIGCHLD, &sa, NULL) < 0)
    return -1;
  return 0;
}

static void
reap_children(void)
{
  while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static int
usage(FILE *out, const char *argv0)
{
  fprintf(out, "usage: %s [-c FILE] [-C] [-v]\n", argv0);
  return out == stdout ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char **argv)
{
  RawkeysConfig cfg;
  RawkeysRawm rawm;
  RawkeysRawpanel panel;
  RawkeysXkb xkb;
  const char *config_path = NULL;
  int check = 0, running = 1, restart = 0, i;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-c") && i + 1 < argc) config_path = argv[++i];
    else if (!strcmp(argv[i], "-C")) check = 1;
    else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      printf("rawkeys %s\n", VERSION); return EXIT_SUCCESS;
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
      return usage(stdout, argv[0]);
    else return usage(stderr, argv[0]);
  }

  rawkeys_config_init(&cfg);
  if (rawkeys_config_load(&cfg, config_path, stderr) < 0 ||
      rawkeys_config_validate(&cfg, stderr) < 0) {
    rawkeys_config_destroy(&cfg); return EXIT_FAILURE;
  }
  if (check) { rawkeys_config_destroy(&cfg); return EXIT_SUCCESS; }

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "rawkeys: cannot open display %s\n", XDisplayName(NULL));
    rawkeys_config_destroy(&cfg); return EXIT_FAILURE;
  }
  XSetErrorHandler(xerror);
  screen_no = DefaultScreen(dpy);
  root = RootWindow(dpy, screen_no);
  if (rawkeys_rawm_init(&rawm, dpy, screen_no) < 0 ||
      rawkeys_rawpanel_init(&panel, dpy, screen_no) < 0 ||
      rawkeys_xkb_init(&xkb, dpy, cfg.per_window, stderr) < 0 ||
      install_signals() < 0 || authority_init() < 0 || grabkeys(&cfg) < 0) {
    rawkeys_config_destroy(&cfg); XCloseDisplay(dpy); return EXIT_FAILURE;
  }

  {
    Window focused = None;
    if (rawkeys_rawm_focused(&rawm, &focused, NULL) == 0)
      (void)rawkeys_xkb_focus(&xkb, focused, stderr);
  }

  while (running) {
    struct pollfd fds[2];
    int ready;
    fds[0].fd = ConnectionNumber(dpy); fds[0].events = POLLIN; fds[0].revents = 0;
    fds[1].fd = signalpipe[0]; fds[1].events = POLLIN; fds[1].revents = 0;
    ready = poll(fds, 2, 1000);
    if (ready < 0 && errno != EINTR) {
      fprintf(stderr, "rawkeys: poll: %s\n", strerror(errno)); break;
    }
    if (fds[1].revents & POLLIN) {
      unsigned char sigs[64];
      ssize_t n, k;
      while ((n = read(signalpipe[0], sigs, sizeof(sigs))) > 0)
        for (k = 0; k < n; ++k) {
          if (sigs[k] == SIGCHLD) reap_children();
          else if (sigs[k] == SIGHUP) { restart = 1; running = 0; }
          else if (sigs[k] == SIGTERM || sigs[k] == SIGINT) running = 0;
        }
    }
    while (XPending(dpy)) {
      XEvent event;
      XNextEvent(dpy, &event);
      if (event.type == KeyPress) {
        const RawkeysBinding *b = find_binding(&cfg, &event.xkey);
        if (b) (void)dispatch_binding(&cfg, b, &rawm, &panel);
      } else if (event.type == MappingNotify) {
        XRefreshKeyboardMapping(&event.xmapping);
        if (event.xmapping.request == MappingKeyboard && grabkeys(&cfg) < 0)
          running = 0;
      }
      if (rawkeys_rawm_event(&rawm, &event)) {
        Window focused = None;
        if (event.type == DestroyNotify) {
          rawm.owner = None;
          (void)rawkeys_xkb_focus(&xkb, None, stderr);
        } else if (rawkeys_rawm_focused(&rawm, &focused, NULL) == 0)
          (void)rawkeys_xkb_focus(&xkb, focused, stderr);
      }
    }
    {
      Window focused = None;
      int refreshed = rawkeys_rawm_refresh(&rawm, NULL);
      if (refreshed > 0 && rawkeys_rawm_focused(&rawm, &focused, NULL) == 0)
        (void)rawkeys_xkb_focus(&xkb, focused, stderr);
      else if (refreshed == 0)
        (void)rawkeys_xkb_focus(&xkb, None, stderr);
    }
  }

  signal_write_fd = -1;
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  if (authority_window != None)
    XDestroyWindow(dpy, authority_window);
  XSync(dpy, False);
  XCloseDisplay(dpy);
  close(signalpipe[0]); close(signalpipe[1]);
  rawkeys_config_destroy(&cfg);
  if (restart) {
    execvp(argv[0], argv);
    fprintf(stderr, "rawkeys: restart execvp %s: %s\n", argv[0], strerror(errno));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
