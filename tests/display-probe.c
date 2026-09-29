#include <X11/Xlib.h>

#include <stdlib.h>

int
main(void)
{
  Display *display = XOpenDisplay(NULL);
  if (!display)
    return EXIT_FAILURE;
  XCloseDisplay(display);
  return EXIT_SUCCESS;
}
