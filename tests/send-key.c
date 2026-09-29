#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef Bool (*FakeKeyEvent)(Display *, unsigned int, Bool, unsigned long);

int
main(int argc, char **argv)
{
  Display *dpy;
  void *handle;
  FakeKeyEvent fake;
  KeyCode *codes;
  int i;

  if (argc < 2) {
    fprintf(stderr, "usage: %s KEYSYM [KEYSYM ...]\n", argv[0]);
    return EXIT_FAILURE;
  }
  dpy = XOpenDisplay(NULL);
  if (!dpy) return EXIT_FAILURE;
  codes = calloc((size_t)(argc - 1), sizeof(*codes));
  if (!codes) return EXIT_FAILURE;
  for (i = 1; i < argc; ++i) {
    KeySym sym = XStringToKeysym(argv[i]);
    codes[i - 1] = sym == NoSymbol ? 0 : XKeysymToKeycode(dpy, sym);
    if (!codes[i - 1]) return EXIT_FAILURE;
  }
  handle = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    free(codes);
    XCloseDisplay(dpy);
    return 77;
  }
  *(void **)(&fake) = dlsym(handle, "XTestFakeKeyEvent");
  if (!fake) {
    dlclose(handle);
    free(codes);
    XCloseDisplay(dpy);
    return 77;
  }
  for (i = 0; i < argc - 1; ++i)
    fake(dpy, codes[i], True, CurrentTime);
  for (i = argc - 2; i >= 0; --i)
    fake(dpy, codes[i], False, CurrentTime);
  XSync(dpy, False);
  free(codes);
  XCloseDisplay(dpy);
  dlclose(handle);
  return EXIT_SUCCESS;
}
