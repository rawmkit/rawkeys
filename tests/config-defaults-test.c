#include "config.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
  RawkeysConfig defaults;
  RawkeysConfig loaded;
  int ok;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s RAWKEYS.CONF\n", argv[0]);
    return EXIT_FAILURE;
  }

  rawkeys_config_init(&defaults);
  rawkeys_config_init(&loaded);
  if (rawkeys_config_load(&loaded, argv[1], stderr) < 0)
  {
    rawkeys_config_destroy(&loaded);
    rawkeys_config_destroy(&defaults);
    return EXIT_FAILURE;
  }

  ok = defaults.per_window == loaded.per_window &&
       defaults.command_count == loaded.command_count &&
       defaults.binding_count == loaded.binding_count;
  rawkeys_config_destroy(&loaded);
  rawkeys_config_destroy(&defaults);
  if (!ok)
    fputs("rawkeys compiled defaults differ from installed config\n", stderr);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
