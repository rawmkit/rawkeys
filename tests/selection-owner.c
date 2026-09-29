#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
  Display *dpy;
  Atom selection;
  if (argc != 2) {
    fprintf(stderr, "usage: %s SELECTION\n", argv[0]);
    return EXIT_FAILURE;
  }
  dpy = XOpenDisplay(NULL);
  if (!dpy) return EXIT_FAILURE;
  selection = XInternAtom(dpy, argv[1], True);
  if (selection == None || XGetSelectionOwner(dpy, selection) == None) {
    XCloseDisplay(dpy);
    return EXIT_FAILURE;
  }
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
