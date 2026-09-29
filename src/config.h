#ifndef RAWKEYS_CONFIG_H
#define RAWKEYS_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
  RAWKEYS_TARGET_EXEC,
  RAWKEYS_TARGET_RAWM,
  RAWKEYS_TARGET_RAWPANEL
} RawkeysTarget;

typedef enum {
  RAWKEYS_MOD_SHIFT   = 1u << 0,
  RAWKEYS_MOD_CONTROL = 1u << 1,
  RAWKEYS_MOD_1       = 1u << 2,
  RAWKEYS_MOD_2       = 1u << 3,
  RAWKEYS_MOD_3       = 1u << 4,
  RAWKEYS_MOD_4       = 1u << 5,
  RAWKEYS_MOD_5       = 1u << 6
} RawkeysModifier;

typedef struct {
  char *name;
  char **argv;
  size_t argc;
} RawkeysCommand;

typedef struct {
  unsigned int modifiers;
  char *key;
  RawkeysTarget target;
  char *action;
  char *command;
  char *capture;
  bool has_number;
  int number;
  bool has_tag;
  bool all_tags;
  unsigned int tag;
  bool has_layout;
  char *layout;
  bool has_delta;
  double delta;
  bool has_factor;
  double factor;
  bool restart;
} RawkeysBinding;

typedef struct {
  bool per_window;
  RawkeysCommand *commands;
  size_t command_count;
  RawkeysBinding *bindings;
  size_t binding_count;
} RawkeysConfig;

void rawkeys_config_init(RawkeysConfig *cfg);
void rawkeys_config_destroy(RawkeysConfig *cfg);
int rawkeys_config_load(RawkeysConfig *cfg, const char *explicit_path, FILE *err);
int rawkeys_config_validate(const RawkeysConfig *cfg, FILE *err);
const RawkeysCommand *rawkeys_config_find_command(const RawkeysConfig *cfg,
                                                   const char *name);

#endif
