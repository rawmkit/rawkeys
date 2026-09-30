#include "config.h"

#include <libconfig.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static char *
dupstr(const char *s)
{
  char *p = strdup(s ? s : "");
  return p;
}

static void
free_command(RawkeysCommand *cmd)
{
  size_t i;
  if (!cmd) return;
  free(cmd->name);
  for (i = 0; i < cmd->argc; ++i) free(cmd->argv[i]);
  free(cmd->argv);
  memset(cmd, 0, sizeof(*cmd));
}

static void
free_binding(RawkeysBinding *b)
{
  if (!b) return;
  free(b->key);
  free(b->action);
  free(b->command);
  free(b->capture);
  free(b->layout);
  memset(b, 0, sizeof(*b));
}

void
rawkeys_config_init(RawkeysConfig *cfg)
{
  memset(cfg, 0, sizeof(*cfg));
  cfg->per_window = true;
}

void
rawkeys_config_destroy(RawkeysConfig *cfg)
{
  size_t i;
  if (!cfg) return;
  for (i = 0; i < cfg->command_count; ++i) free_command(&cfg->commands[i]);
  for (i = 0; i < cfg->binding_count; ++i) free_binding(&cfg->bindings[i]);
  free(cfg->commands);
  free(cfg->bindings);
  memset(cfg, 0, sizeof(*cfg));
}

const RawkeysCommand *
rawkeys_config_find_command(const RawkeysConfig *cfg, const char *name)
{
  size_t i;
  if (!cfg || !name) return NULL;
  for (i = 0; i < cfg->command_count; ++i)
    if (!strcmp(cfg->commands[i].name, name)) return &cfg->commands[i];
  return NULL;
}

static int
cfg_error(FILE *err, const config_setting_t *s, const char *message,
          const char *value)
{
  if (err) {
    const char *file = s ? config_setting_source_file(s) : NULL;
    int line = s ? config_setting_source_line(s) : 0;
    fputs("rawkeys: configuration error", err);
    if (file && *file) fprintf(err, " in %s", file);
    if (line > 0) fprintf(err, ":%d", line);
    fputs(": ", err);
    fprintf(err, message, value ? value : "");
    fputc('\n', err);
  }
  return -1;
}

static int
name_allowed(const char *name, const char *const *allowed, size_t count)
{
  size_t i;
  for (i = 0; i < count; ++i)
    if (!strcmp(name, allowed[i])) return 1;
  return 0;
}

static int
check_allowed(FILE *err, const config_setting_t *group,
              const char *const *allowed, size_t count)
{
  int i, n = config_setting_length(group);
  for (i = 0; i < n; ++i) {
    config_setting_t *item = config_setting_get_elem(group, (unsigned int)i);
    const char *name = item ? config_setting_name(item) : NULL;
    if (name && !name_allowed(name, allowed, count))
      return cfg_error(err, item, "unknown setting '%s'", name);
  }
  return 0;
}

static int
parse_modifier(const char *s, unsigned int *mask)
{
  if (!strcmp(s, "Shift")) *mask |= RAWKEYS_MOD_SHIFT;
  else if (!strcmp(s, "Control")) *mask |= RAWKEYS_MOD_CONTROL;
  else if (!strcmp(s, "Mod1")) *mask |= RAWKEYS_MOD_1;
  else if (!strcmp(s, "Mod2")) *mask |= RAWKEYS_MOD_2;
  else if (!strcmp(s, "Mod3")) *mask |= RAWKEYS_MOD_3;
  else if (!strcmp(s, "Mod4")) *mask |= RAWKEYS_MOD_4;
  else if (!strcmp(s, "Mod5")) *mask |= RAWKEYS_MOD_5;
  else return -1;
  return 0;
}

static int
parse_modifiers(FILE *err, const config_setting_t *entry, unsigned int *mask)
{
  config_setting_t *mods = config_setting_get_member(entry, "modifiers");
  int i, n;
  *mask = 0;
  if (!mods || config_setting_type(mods) != CONFIG_TYPE_LIST)
    return cfg_error(err, entry, "binding requires list member '%s'", "modifiers");
  n = config_setting_length(mods);
  for (i = 0; i < n; ++i) {
    config_setting_t *m = config_setting_get_elem(mods, (unsigned int)i);
    const char *name;
    if (!m || config_setting_type(m) != CONFIG_TYPE_STRING)
      return cfg_error(err, mods, "modifier names must be strings%s", "");
    name = config_setting_get_string(m);
    if (parse_modifier(name, mask) < 0)
      return cfg_error(err, m, "unknown modifier '%s'", name);
  }
  return 0;
}

static RawkeysTarget
parse_target(const char *s, int *ok)
{
  *ok = 1;
  if (!strcmp(s, "exec")) return RAWKEYS_TARGET_EXEC;
  if (!strcmp(s, "rawm")) return RAWKEYS_TARGET_RAWM;
  if (!strcmp(s, "rawpanel")) return RAWKEYS_TARGET_RAWPANEL;
  *ok = 0;
  return RAWKEYS_TARGET_EXEC;
}

static int
replace_commands(FILE *err, RawkeysConfig *cfg, config_setting_t *group)
{
  RawkeysCommand *items = NULL;
  size_t count = 0;
  int i, n;

  if (config_setting_type(group) != CONFIG_TYPE_GROUP)
    return cfg_error(err, group, "%s must be a group", "commands");
  n = config_setting_length(group);
  if (n > 0) {
    items = calloc((size_t)n, sizeof(*items));
    if (!items) return -1;
  }
  for (i = 0; i < n; ++i) {
    config_setting_t *entry = config_setting_get_elem(group, (unsigned int)i);
    const char *name = config_setting_name(entry);
    int j, argc;
    if (!name || config_setting_type(entry) != CONFIG_TYPE_LIST) {
      cfg_error(err, entry, "command '%s' must be a list", name ? name : "");
      goto fail;
    }
    argc = config_setting_length(entry);
    if (argc <= 0) {
      cfg_error(err, entry, "command '%s' must not be empty", name);
      goto fail;
    }
    items[count].name = dupstr(name);
    items[count].argv = calloc((size_t)argc + 1u, sizeof(char *));
    if (!items[count].name || !items[count].argv) goto fail;
    items[count].argc = (size_t)argc;
    for (j = 0; j < argc; ++j) {
      config_setting_t *arg = config_setting_get_elem(entry, (unsigned int)j);
      if (!arg || config_setting_type(arg) != CONFIG_TYPE_STRING) {
        cfg_error(err, entry, "command '%s' arguments must be strings", name);
        goto fail;
      }
      items[count].argv[j] = dupstr(config_setting_get_string(arg));
      if (!items[count].argv[j]) goto fail;
    }
    ++count;
  }
  for (i = 0; i < (int)cfg->command_count; ++i) free_command(&cfg->commands[i]);
  free(cfg->commands);
  cfg->commands = items;
  cfg->command_count = count;
  return 0;
fail:
  for (i = 0; i < n; ++i) free_command(&items[i]);
  free(items);
  return -1;
}

static int
get_optional_string(config_setting_t *entry, const char *name, char **out,
                    FILE *err)
{
  config_setting_t *s = config_setting_get_member(entry, name);
  if (!s) return 0;
  if (config_setting_type(s) != CONFIG_TYPE_STRING)
    return cfg_error(err, s, "%s must be a string", name);
  *out = dupstr(config_setting_get_string(s));
  return *out ? 0 : -1;
}

static int
replace_bindings(FILE *err, RawkeysConfig *cfg, config_setting_t *list)
{
  RawkeysBinding *items = NULL;
  size_t count = 0;
  int i, n;

  if (config_setting_type(list) != CONFIG_TYPE_LIST)
    return cfg_error(err, list, "%s must be a list", "bindings");
  n = config_setting_length(list);
  if (n > 0) {
    items = calloc((size_t)n, sizeof(*items));
    if (!items) return -1;
  }
  for (i = 0; i < n; ++i) {
    RawkeysBinding *b = &items[count];
    config_setting_t *entry = config_setting_get_elem(list, (unsigned int)i);
    config_setting_t *s;
    const char *value;
    int ok;
    if (!entry || config_setting_type(entry) != CONFIG_TYPE_GROUP) {
      cfg_error(err, entry, "binding entries must be groups%s", "");
      goto fail;
    }
    {
      static const char *const allowed[] = {
        "modifiers", "key", "target", "action", "command", "capture",
        "number", "tag", "layout", "delta", "factor", "restart"
      };
      if (check_allowed(err, entry, allowed, sizeof(allowed) / sizeof(allowed[0])) < 0)
        goto fail;
    }
    if (parse_modifiers(err, entry, &b->modifiers) < 0) goto fail;
    s = config_setting_get_member(entry, "key");
    if (!s || config_setting_type(s) != CONFIG_TYPE_STRING) {
      cfg_error(err, entry, "binding requires string member '%s'", "key");
      goto fail;
    }
    b->key = dupstr(config_setting_get_string(s));
    s = config_setting_get_member(entry, "target");
    if (!s || config_setting_type(s) != CONFIG_TYPE_STRING) {
      cfg_error(err, entry, "binding requires string member '%s'", "target");
      goto fail;
    }
    value = config_setting_get_string(s);
    b->target = parse_target(value, &ok);
    if (!ok) {
      cfg_error(err, s, "unknown target '%s'", value);
      goto fail;
    }
    if (get_optional_string(entry, "action", &b->action, err) < 0 ||
        get_optional_string(entry, "command", &b->command, err) < 0 ||
        get_optional_string(entry, "capture", &b->capture, err) < 0 ||
        get_optional_string(entry, "layout", &b->layout, err) < 0)
      goto fail;
    s = config_setting_get_member(entry, "number");
    if (s) {
      if (config_setting_type(s) != CONFIG_TYPE_INT) {
        cfg_error(err, s, "%s must be an integer", "number"); goto fail;
      }
      b->has_number = true; b->number = config_setting_get_int(s);
    }
    s = config_setting_get_member(entry, "tag");
    if (s) {
      if (config_setting_type(s) == CONFIG_TYPE_INT) {
        int tag = config_setting_get_int(s);
        if (tag <= 0) { cfg_error(err, s, "%s must be positive or 'all'", "tag"); goto fail; }
        b->has_tag = true; b->tag = (unsigned int)tag;
      } else if (config_setting_type(s) == CONFIG_TYPE_STRING &&
                 !strcmp(config_setting_get_string(s), "all")) {
        b->has_tag = true; b->all_tags = true;
      } else { cfg_error(err, s, "%s must be positive or 'all'", "tag"); goto fail; }
    }
    s = config_setting_get_member(entry, "delta");
    if (s) {
      if (config_setting_type(s) != CONFIG_TYPE_FLOAT &&
          config_setting_type(s) != CONFIG_TYPE_INT) {
        cfg_error(err, s, "%s must be numeric", "delta"); goto fail;
      }
      b->has_delta = true;
      b->delta = config_setting_type(s) == CONFIG_TYPE_INT
               ? (double)config_setting_get_int(s) : config_setting_get_float(s);
    }
    s = config_setting_get_member(entry, "factor");
    if (s) {
      if (config_setting_type(s) != CONFIG_TYPE_FLOAT &&
          config_setting_type(s) != CONFIG_TYPE_INT) {
        cfg_error(err, s, "%s must be numeric", "factor"); goto fail;
      }
      b->has_factor = true;
      b->factor = config_setting_type(s) == CONFIG_TYPE_INT
                ? (double)config_setting_get_int(s) : config_setting_get_float(s);
    }
    s = config_setting_get_member(entry, "restart");
    if (s) {
      if (config_setting_type(s) != CONFIG_TYPE_BOOL) {
        cfg_error(err, s, "%s must be boolean", "restart"); goto fail;
      }
      b->restart = config_setting_get_bool(s) != 0;
    }
    ++count;
  }
  for (i = 0; i < (int)cfg->binding_count; ++i) free_binding(&cfg->bindings[i]);
  free(cfg->bindings);
  cfg->bindings = items;
  cfg->binding_count = count;
  return 0;
fail:
  for (i = 0; i < n; ++i) free_binding(&items[i]);
  free(items);
  return -1;
}

static int
apply_config(FILE *err, RawkeysConfig *cfg, const char *path)
{
  config_t c;
  config_setting_t *root, *s, *xkb;
  int result = -1;

  config_init(&c);
  if (!config_read_file(&c, path)) {
    if (err) fprintf(err, "rawkeys: %s:%d: %s\n", path,
                     config_error_line(&c), config_error_text(&c));
    config_destroy(&c);
    return -1;
  }
  root = config_root_setting(&c);
  {
    static const char *const allowed[] = { "xkb", "commands", "bindings" };
    if (check_allowed(err, root, allowed, sizeof(allowed) / sizeof(allowed[0])) < 0)
      goto out;
  }
  xkb = config_setting_get_member(root, "xkb");
  if (xkb) {
    if (config_setting_type(xkb) != CONFIG_TYPE_GROUP) {
      cfg_error(err, xkb, "%s must be a group", "xkb"); goto out;
    }
    {
      static const char *const allowed[] = { "per_window" };
      if (check_allowed(err, xkb, allowed, sizeof(allowed) / sizeof(allowed[0])) < 0)
        goto out;
    }
    s = config_setting_get_member(xkb, "per_window");
    if (s) {
      if (config_setting_type(s) != CONFIG_TYPE_BOOL) {
        cfg_error(err, s, "%s must be boolean", "xkb.per_window"); goto out;
      }
      cfg->per_window = config_setting_get_bool(s) != 0;
    }
  }
  s = config_setting_get_member(root, "commands");
  if (s && replace_commands(err, cfg, s) < 0) goto out;
  s = config_setting_get_member(root, "bindings");
  if (s && replace_bindings(err, cfg, s) < 0) goto out;
  result = 0;
out:
  config_destroy(&c);
  return result;
}

static int
file_readable(const char *path)
{
  return path && access(path, R_OK) == 0;
}

int
rawkeys_config_load(RawkeysConfig *cfg, const char *explicit_path, FILE *err)
{
  char system[PATH_MAX];
  char user[PATH_MAX];
  const char *xdg, *home;
  int n;

  if (explicit_path)
    return apply_config(err, cfg, explicit_path);

  n = snprintf(system, sizeof(system), "%s/rawkeys/rawkeys.conf", SYSCONFDIR);
  if (n < 0 || (size_t)n >= sizeof(system))
    return -1;
  if (file_readable(system) && apply_config(err, cfg, system) < 0)
    return -1;

  xdg = getenv("XDG_CONFIG_HOME");
  home = getenv("HOME");
  if (xdg && xdg[0] == '/') {
    n = snprintf(user, sizeof(user), "%s/rawkeys/rawkeys.conf", xdg);
    if (n < 0 || (size_t)n >= sizeof(user)) return -1;
  } else if (home && home[0] == '/') {
    n = snprintf(user, sizeof(user), "%s/.config/rawkeys/rawkeys.conf", home);
    if (n < 0 || (size_t)n >= sizeof(user)) return -1;
  } else return 0;
  if (file_readable(user) && apply_config(err, cfg, user) < 0) return -1;
  return 0;
}

static int
rawm_action_known(const char *s)
{
  static const char *const names[] = {
    "focus_stack", "focus_index", "inc_master", "set_master_factor", "zoom",
    "view", "toggle_view", "cycle_view", "kill_client", "set_layout",
    "cycle_layout", "toggle_floating", "tag", "toggle_tag", "window_view",
    "toggle_fullscreen", "focus_monitor", "tag_monitor", "rename_tag", "quit"
  };
  size_t i;
  for (i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    if (!strcmp(s, names[i])) return 1;
  return 0;
}

static int
panel_action_known(const char *s)
{
  return s && (!strcmp(s, "toggle_visibility") || !strcmp(s, "volume_up") ||
               !strcmp(s, "volume_down") || !strcmp(s, "volume_mute"));
}

static int
layout_known(const char *layout)
{
  static const char *const names[] = {
    "tile", "floating", "monocle", "bstack", "bstack_horizontal", "grid"
  };
  size_t i;
  if (!layout) return 0;
  for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    if (!strcmp(layout, names[i])) return 1;
  return 0;
}

static int
has_extra_args(const RawkeysBinding *b)
{
  return b->has_number || b->has_tag || b->has_layout || b->has_delta ||
         b->has_factor || b->restart;
}

static int
validate_rawm_binding(const RawkeysConfig *cfg, const RawkeysBinding *b, FILE *err)
{
  if (!b->action || !rawm_action_known(b->action)) {
    if (err) fprintf(err, "rawkeys: configuration error: unknown rawm action '%s'\n",
                     b->action ? b->action : "");
    return -1;
  }
  if (!strcmp(b->action, "rename_tag")) {
    if (!b->capture || !rawkeys_config_find_command(cfg, b->capture) ||
        has_extra_args(b)) {
      if (err) fputs("rawkeys: configuration error: rename_tag requires only a known capture command\n", err);
      return -1;
    }
    return 0;
  }
  if (b->capture) {
    if (err) fputs("rawkeys: configuration error: capture is valid only for rawm rename_tag\n", err);
    return -1;
  }

  if (!strcmp(b->action, "focus_stack") || !strcmp(b->action, "cycle_view") ||
      !strcmp(b->action, "cycle_layout") || !strcmp(b->action, "focus_monitor") ||
      !strcmp(b->action, "tag_monitor")) {
    if (!b->has_number || (b->number != -1 && b->number != 1) ||
        b->has_tag || b->has_layout || b->has_delta || b->has_factor || b->restart)
      goto badargs;
  } else if (!strcmp(b->action, "focus_index")) {
    if (!b->has_number || b->number <= 0 || b->has_tag || b->has_layout ||
        b->has_delta || b->has_factor || b->restart)
      goto badargs;
  } else if (!strcmp(b->action, "inc_master")) {
    if (!b->has_number || b->number == 0 || b->has_tag || b->has_layout ||
        b->has_delta || b->has_factor || b->restart)
      goto badargs;
  } else if (!strcmp(b->action, "set_master_factor")) {
    if (b->has_number || b->has_tag || b->has_layout || b->restart ||
        (b->has_delta == b->has_factor) ||
        (b->has_factor && (b->factor < 0.1 || b->factor > 0.9)) ||
        (b->has_delta && b->delta == 0.0))
      goto badargs;
  } else if (!strcmp(b->action, "view")) {
    if (b->has_number || b->has_layout || b->has_delta || b->has_factor || b->restart)
      goto badargs;
  } else if (!strcmp(b->action, "toggle_view") || !strcmp(b->action, "tag") ||
             !strcmp(b->action, "toggle_tag")) {
    if (!b->has_tag || b->has_number || b->has_layout || b->has_delta ||
        b->has_factor || b->restart)
      goto badargs;
  } else if (!strcmp(b->action, "set_layout")) {
    if (b->has_number || b->has_tag || b->has_delta || b->has_factor || b->restart ||
        (b->has_layout && !layout_known(b->layout)))
      goto badargs;
  } else if (!strcmp(b->action, "quit")) {
    if (b->has_number || b->has_tag || b->has_layout || b->has_delta || b->has_factor)
      goto badargs;
  } else if (has_extra_args(b)) {
    goto badargs;
  }
  return 0;

badargs:
  if (err) fprintf(err, "rawkeys: configuration error: invalid arguments for rawm action '%s'\n",
                   b->action);
  return -1;
}

int
rawkeys_config_validate(const RawkeysConfig *cfg, FILE *err)
{
  size_t i, j;
  if (!cfg) return -1;
  for (i = 0; i < cfg->binding_count; ++i) {
    const RawkeysBinding *b = &cfg->bindings[i];
    if (!b->key || !*b->key) {
      if (err) fputs("rawkeys: configuration error: binding key is empty\n", err);
      return -1;
    }
    for (j = 0; j < i; ++j)
      if (cfg->bindings[j].modifiers == b->modifiers &&
          !strcmp(cfg->bindings[j].key, b->key)) {
        if (err) fprintf(err, "rawkeys: configuration error: duplicate binding for '%s'\n", b->key);
        return -1;
      }
    if (b->target == RAWKEYS_TARGET_EXEC) {
      if (!b->command || !rawkeys_config_find_command(cfg, b->command)) {
        if (err) fprintf(err, "rawkeys: configuration error: exec binding references unknown command '%s'\n",
                         b->command ? b->command : "");
        return -1;
      }
      if (b->action || b->capture || has_extra_args(b)) {
        if (err) fputs("rawkeys: configuration error: exec binding accepts command only\n", err);
        return -1;
      }
    } else if (b->target == RAWKEYS_TARGET_RAWM) {
      if (b->command || validate_rawm_binding(cfg, b, err) < 0) return -1;
    } else if (b->target == RAWKEYS_TARGET_RAWPANEL) {
      if (b->command || b->capture || has_extra_args(b) || !panel_action_known(b->action)) {
        if (err) fprintf(err, "rawkeys: configuration error: invalid rawpanel action '%s'\n",
                         b->action ? b->action : "");
        return -1;
      }
    }
  }
  return 0;
}
