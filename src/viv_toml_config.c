#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <toml.h>

#include <wlr/util/log.h>

#include "viv_config_support.h"
#include "viv_config_types.h"
#include "viv_toml_config.h"
#include "viv_types.h"

#define ERRBUF_SIZE 200

struct pair {
    char *key;
    uint32_t value;
};

#define NULL_PAIR {"", 0}
#define IS_NULL_PAIR(ROW) (strlen(ROW) == 0)
#define MAX_MAP_LEN 1000

#define GLOBAL_META_STRING "meta"

static struct pair meta_map[] = {
    {GLOBAL_META_STRING, 0u},
    {"shift", WLR_MODIFIER_SHIFT},
    {"caps", WLR_MODIFIER_CAPS},
    {"ctrl", WLR_MODIFIER_CTRL},
    {"alt", WLR_MODIFIER_ALT},
    {"mod2", WLR_MODIFIER_MOD2},
    {"mod3", WLR_MODIFIER_MOD3},
    {"logo", WLR_MODIFIER_LOGO},
    {"mod5", WLR_MODIFIER_MOD5},
    NULL_PAIR,
};

static struct pair cursor_button_map[] = {
    {"left", VIV_LEFT_BUTTON},
    {"right", VIV_RIGHT_BUTTON},
    {"middle", VIV_MIDDLE_BUTTON},
    NULL_PAIR,
};

static bool is_null_pair(struct pair *row) {
    return (strlen(row->key) == 0);
}

static bool look_up_string_in_map(char *string, struct pair *map, uint32_t *result) {
    for (size_t i = 0; i < MAX_MAP_LEN; i++) {
        struct pair row = map[i];
        if (is_null_pair(&row)) {
            break;
        }

        if (strcmp(string, row.key) == 0) {
            *result = row.value;
            wlr_log(WLR_DEBUG, "Parsed string %s as value %d", string, *result);
            return true;
        }
    }
    return false;
}

static void parse_config_bool(toml_table_t *root, char *section_name, char *key_name, bool *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_bool_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as bool", section_name, key_name);
    }
    *target = key.u.b;
    wlr_log(WLR_DEBUG, "Parsing config %s.%s = %d as bool", section_name, key_name, *target);
}

static void parse_config_int(toml_table_t *root, char *section_name, char *key_name, int *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_int_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as int", section_name, key_name);
    }
    *target = key.u.i;
    wlr_log(WLR_DEBUG, "Parsing config %s.%s = %d as int", section_name, key_name, *target);
}

static void parse_config_double(toml_table_t *root, char *section_name, char *key_name, double *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_double_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as double", section_name, key_name);
    }
    *target = key.u.d;
    wlr_log(WLR_DEBUG, "Parsing config %s.%s = %f as double", section_name, key_name, *target);
}

static void parse_config_string_map(toml_table_t *root, char *section_name, char *key_name, struct pair *map, uint32_t *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_string_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as string", section_name, key_name);
    }
    wlr_log(WLR_DEBUG, "Parsing config %s.%s = \"%s\" as string", section_name, key_name, key.u.s);

    bool success = look_up_string_in_map(key.u.s, map, target);
    if (!success) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s: value \"%s\" not valid",
                                    section_name, key_name, key.u.s);
    }
    free(key.u.s);
}

static void parse_config_string_raw(toml_table_t *root, char *section_name, char *key_name, char **target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_string_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as string", section_name, key_name);
    }
    *target = key.u.s;
    wlr_log(WLR_DEBUG, "Parsing config %s.%s = \"%s\" as string", section_name, key_name, *target);

    // TODO: save information about the fact that this is malloc'd, so that we know if we need to free it later
}

static void parse_config_array_fixed_size_float_float(toml_table_t *root, char *section_name, char *key_name, int array_len, float *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_array_t *key = toml_array_in(section, key_name);
    if (!(toml_array_kind(key) == 'v')) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as array of values, got type '%c'",
                                    section_name, key_name, toml_array_kind(key));
    }

    if (!(toml_array_nelem(key) == array_len)) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error parsing %s.%s as array: expected %d entries, got %d",
                                    section_name, key_name, array_len, toml_array_nelem(key));
    }

    if (!(toml_array_type(key) == 'd')) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as float type array", section_name, key_name);
    }

    for (size_t i = 0; i < (size_t)array_len; i++) {
        toml_datum_t value = toml_double_at(key, i);
        if (!value.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing %s.%s array as float type",
                                        section_name, key_name);
        }
        target[i] = (float)value.u.d;
        wlr_log(WLR_DEBUG, "Parsing %s.%s[%d] = %f as float", section_name, key_name, (uint32_t)i, target[i]);
    }
}

static void parse_config_array_variable_length(toml_table_t *root, char *section_name, void (*parse_func)(toml_table_t *keybind_table, struct viv_keybind *keybind)) {
    toml_array_t *array = toml_array_in(root, section_name);
    wlr_log(WLR_INFO, "Config section %s is an array of kind %c with %d items",
            section_name, toml_array_kind(array), toml_array_nelem(array));

    if (toml_array_kind(array) != 't') {
        EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[%s]]: expected array of type 't', got type %c",
                                    toml_array_kind(array));
    }

    uint32_t our_array_nelem = toml_array_nelem(array) + 1;
    struct viv_keybind *the_keybinds = calloc(our_array_nelem, sizeof(struct viv_keybind));

    for (size_t i = 0; i < our_array_nelem - 1; i++) {
        parse_func(toml_table_at(array, i), &the_keybinds[i]);
    }
}

// TODO: Can't we autogenerate some/all of this?
// We could do: if num var args > 0, call an autogenerated function to
// fill in the parameters, that way we get compiler help to make sure
// we implemented all the functions
static void parse_mappable_arguments(toml_table_t *table, char *action, union viv_mappable_payload *payload) {
    if (strcmp(action, "do_exec") == 0) {
        toml_datum_t executable_string = toml_string_in(table, "executable");
        if (!executable_string.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"executable\" key",
                                        action);
        }
        strncpy(payload->do_exec.executable, executable_string.u.s, 100);
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = \"%s\" for action \"%s\"",
                "executable", executable_string.u.s, action);
    } else if (strcmp(action, "do_shell") == 0) {
        toml_datum_t command_string = toml_string_in(table, "command");
        if (!command_string.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"command\" key",
                                        action);
        }
        strncpy(payload->do_shell.command, command_string.u.s, 100);
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = \"%s\" for action \"%s\"",
                "command", command_string.u.s, action);
    } else if (strcmp(action, "increment_divide") == 0) {
        toml_datum_t increment_double = toml_double_in(table, "increment");
        if (!increment_double.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"increment\" key",
                                        action);
        }
        payload->increment_divide.increment = increment_double.u.d;
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = %f for action \"%s\"",
                "increment", increment_double.u.d, action);
    } else if (strcmp(action, "shift_active_window_to_workspace") == 0) {
        toml_datum_t workspace_name_string = toml_string_in(table, "workspace_name");
        if (!workspace_name_string.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"workspace_name\" key",
                                        action);
        }
        strncpy(payload->shift_active_window_to_workspace.workspace_name, workspace_name_string.u.s, 100);
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = \"%s\" for action \"%s\"",
                "workspace_name", workspace_name_string.u.s, action);
    } else if (strcmp(action, "switch_to_workspace") == 0) {
        toml_datum_t workspace_name_string = toml_string_in(table, "workspace_name");
        if (!workspace_name_string.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"workspace_name\" key",
                                        action);
        }
        strncpy(payload->switch_to_workspace.workspace_name, workspace_name_string.u.s, 100);
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = \"%s\" for action \"%s\"",
                "workspace_name", workspace_name_string.u.s, action);
    } else {
        // If the action doesn't expect any arguments, check we didn't receive any
        for (size_t i = 0; i < (size_t)toml_table_nkval(table); i++) {
            const char *key = toml_key_in(table, i);
            if (strcmp(key, "action") == 0 || strcmp(key, "keysym") == 0 || strcmp(key, "keycode") == 0) {
                // These arguments are expected
                continue;
            }
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: received argument \"%s\" but no argument expected",
                                        action, key);
        }
    }

}

static void parse_keybind_table(toml_table_t *keybind_table, struct viv_keybind *keybind) {
    toml_datum_t action = toml_string_in(keybind_table, "action");
    toml_datum_t keysym = toml_string_in(keybind_table, "keysym");
    /* toml_datum_t keycode = toml_string_in(keybind_table, "keycode"); */

    // Validate selection of arguments provided
    if (!keysym.ok) {
        EXIT_WITH_MESSAGE("Config error parsing [[keybind]]: no keysym provided");
    }
    if (!action.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for keysym %s: no action provided",
                                    keysym.u.s);
    }

    // Turn the list of modifiers into an appropriate bitmask
    uint32_t modifiers_bitfield = 0u;
    toml_array_t *modifiers = toml_array_in(keybind_table, "modifiers");
    if (modifiers != NULL) {
        wlr_log(WLR_INFO, "keysym %s, modifiers len %d", keysym.u.s, toml_array_nelem(modifiers));
        for (size_t i = 0; i < (size_t)toml_array_nelem(modifiers); i++) {
            toml_datum_t modifier = toml_string_at(modifiers, i);
            uint32_t modifier_code;
            bool success = look_up_string_in_map(modifier.u.s, meta_map, &modifier_code);
            if (!success) {
                EXIT_WITH_FORMATTED_MESSAGE("Config error reading [[keybind]] for keysym %s: modifier %s not valid",
                                            keysym.u.s, modifier.u.s);
            }
            modifiers_bitfield |= modifier_code;
            free(modifier.u.s);
        }
    } else {
        // No modifiers provided => use the default modifiers list ["meta"]
        uint32_t modifier_code;
        bool success = look_up_string_in_map(GLOBAL_META_STRING, meta_map, &modifier_code);
        ASSERT(success);
        modifiers_bitfield |= modifier_code;
    }

    xkb_keysym_t key = xkb_keysym_from_name(keysym.u.s, 0u);
    if (key == XKB_KEY_NoSymbol) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading [[keybind]] for keysym %s: keysym name not recognised",
                                    keysym.u.s);
    }

    keybind->key = key;
    keybind->modifiers = modifiers_bitfield;
    parse_mappable_arguments(keybind_table, action.u.s, &keybind->payload);

    wlr_log(WLR_DEBUG, "Parsed [[keybind]] for keysym %s, modifiers %d, action %s",
            keysym.u.s, modifiers_bitfield, action.u.s);
    free(action.u.s);
    free(keysym.u.s);
}

struct viv_config *viv_load_toml_config(void) {
    UNUSED(parse_config_double);

    struct viv_config *config = (struct viv_config *)(calloc(1, sizeof(struct viv_config)));

    wlr_log(WLR_INFO, "Loading toml config");

    FILE *fp = fopen("/home/sandy/devel/vivarium/config/config.toml", "r");
    if (!fp) {
        EXIT_WITH_MESSAGE("Could not open config.toml");
    }

    char errbuf[ERRBUF_SIZE];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        wlr_log(WLR_ERROR, "Could not parse config.toml:");
        EXIT_WITH_MESSAGE(errbuf);
    }

    toml_table_t *global_config = toml_table_in(root, "global-config");
    if (!global_config) {
        EXIT_WITH_MESSAGE("No [global-config] section found");
    }

    // Do parsing

    // [global-config]
    parse_config_string_map(root, "global-config", "meta-key", meta_map, &config->global_meta_key);
    meta_map[0].value = config->global_meta_key;
    parse_config_bool(root, "global-config", "focus-follows-mouse", &config->focus_follows_mouse);
    parse_config_string_map(root, "global-config", "win-move-cursor-button", cursor_button_map, &config->win_move_cursor_button);
    parse_config_string_map(root, "global-config", "win-resize-cursor-button", cursor_button_map, &config->win_resize_cursor_button);
    parse_config_int(root, "global-config", "border-width", &config->border_width);
    parse_config_array_fixed_size_float_float(root, "global-config", "active-border-colour", 4, config->active_border_colour);
    parse_config_array_fixed_size_float_float(root, "global-config", "inactive-border-colour", 4, config->inactive_border_colour);
    parse_config_int(root, "global-config", "gap-width", &config->gap_width);

    // [background]
    parse_config_string_raw(root, "background", "colour", &config->background.colour);
    parse_config_string_raw(root, "background", "image", &config->background.image);
    parse_config_string_raw(root, "background", "mode", &config->background.mode);

    // [xkb-config]
    parse_config_string_raw(root, "xkb-config", "model", &config->xkb_rules.model);
    parse_config_string_raw(root, "xkb-config", "layout", &config->xkb_rules.layout);
    parse_config_string_raw(root, "xkb-config", "variant", &config->xkb_rules.variant);
    parse_config_string_raw(root, "xkb-config", "options", &config->xkb_rules.options);

    // [debug]
    parse_config_bool(root, "debug", "mark-views-by-shell", &config->debug_mark_views_by_shell);
    parse_config_bool(root, "debug", "mark-active-output", &config->debug_mark_active_output);

    // [[keybind]] list
    /* parse_config_array_variable_length(root, "keybind", NULL); */
    parse_config_array_variable_length(root, "keybind", parse_keybind_table);

    // [[layout]] list
    /* parse_config_array_variable_length(root, "layout", NULL); */
    /* parse_config_array_variable_length(root, "layout", parse_layout); */

    // [[libinput-config]] list
    /* parse_config_array_variable_length(root, "keybind", NULL); */
    /* parse_config_array_variable_length(root, "keybind", parse_libinput_config); */

    toml_free(root);

    /* EXIT_WITH_MESSAGE("done"); */

    return config;
}
