#include <libinput.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <toml.h>

#include <wlr/util/log.h>

#include "viv_layout.h"
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

static struct pair scroll_method_map[] = {
    {"no-scroll", LIBINPUT_CONFIG_SCROLL_NO_SCROLL},
    {"2-finger", LIBINPUT_CONFIG_SCROLL_2FG},
    {"edge", LIBINPUT_CONFIG_SCROLL_EDGE},
    {"on-button-down", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN},
    {"scroll-button", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN},
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

/* static struct viv_keybind *parse_config_array_variable_length(toml_table_t *root, char *section_name, void (*parse_func)(toml_table_t *keybind_table, struct viv_keybind *keybind)) { */
/*     toml_array_t *array = toml_array_in(root, section_name); */
/*     wlr_log(WLR_INFO, "Config section %s is an array of kind %c with %d items", */
/*             section_name, toml_array_kind(array), toml_array_nelem(array)); */

/*     if (toml_array_kind(array) != 't') { */
/*         EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[%s]]: expected array of type 't', got type %c", */
/*                                     section_name, toml_array_kind(array)); */
/*     } */

/*     uint32_t our_array_nelem = toml_array_nelem(array) + 1; */
/*     struct viv_keybind *the_keybinds = calloc(our_array_nelem, sizeof(struct viv_keybind)); */

/*     for (size_t i = 0; i < our_array_nelem - 1; i++) { */
/*         parse_func(toml_table_at(array, i), &the_keybinds[i]); */
/*     } */
/*     static struct viv_keybind null_keybind = TERMINATE_KEYBINDS_LIST(); */
/*     memcpy(&the_keybinds[our_array_nelem - 1], &null_keybind, sizeof(null_keybind)); */

/*     return the_keybinds; */
/* } */

#define PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(ROOT, SECTION_NAME, TYPE, PARSE_FUNC, TARGET) \
    do {                                                                \
        toml_array_t *array = toml_array_in(ROOT, SECTION_NAME);            \
        wlr_log(WLR_INFO, "Config section %s is an array of kind %c with %d items", \
                SECTION_NAME, toml_array_kind(array), toml_array_nelem(array)); \
                                                                            \
        if (toml_array_kind(array) != 't') {                                \
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[%s]]: expected array of type 't', got type %c", \
                                        SECTION_NAME, toml_array_kind(array)); \
        }                                                                   \
                                                                            \
        uint32_t our_array_nelem = toml_array_nelem(array) + 1;             \
        TYPE *the_array = calloc(our_array_nelem, sizeof(TYPE)); \
                                                                            \
        for (size_t i = 0; i < our_array_nelem - 1; i++) {                  \
            PARSE_FUNC(toml_table_at(array, i), &the_array[i]);          \
        }                                                                   \
    } while (false);


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

static void parse_layout_table(toml_table_t *layout_table, struct viv_layout *layout_struct) {
    toml_datum_t layout = toml_string_in(layout_table, "layout");
    if (!layout.ok) {
        EXIT_WITH_MESSAGE("Error parsing [[layout]]: no \"layout\" provided");
    }
    // TODO: Make this a nice autogenerated lookup or something
    char *layout_name = layout.u.s;

    if (strcmp(layout_name, "split") == 0) {
        layout_struct->layout_function = &viv_layout_do_split;
    } else if (strcmp(layout_name, "fullscreen") == 0) {
        layout_struct->layout_function = &viv_layout_do_fullscreen;
    } else if (strcmp(layout_name, "fibonacci_spiral") == 0) {
        layout_struct->layout_function = &viv_layout_do_fibonacci_spiral;
    } else {
        EXIT_WITH_FORMATTED_MESSAGE("Error parsing [[layout]]: layout name \"%s\" not recognised",
                                    layout_name);
    }

    toml_datum_t name = toml_string_in(layout_table, "name");
    if(!name.ok) {
        strncpy(layout_struct->name, layout_name, 100);
    } else {
        strncpy(layout_struct->name, name.u.s, 100);
    }

    toml_datum_t parameter = toml_double_in(layout_table, "parameter");
    if (!parameter.ok) {
        layout_struct->parameter = 0.666;
    } else {
        if (parameter.u.d < 0 || parameter.u.d > 1) {
            EXIT_WITH_FORMATTED_MESSAGE("Error parsing [[layout]] with name \"%s\": parameter %f not between 0 and 1",
                                        layout_struct->name, parameter.u.d);
        }
        layout_struct->parameter = parameter.u.d;
    }

    toml_datum_t counter = toml_double_in(layout_table, "counter");
    if (!counter.ok) {
        layout_struct->counter = 1;
    } else {
        if (counter.u.i < 1) {
            EXIT_WITH_FORMATTED_MESSAGE("Error parsing [[layout]] with name \"%s\": counter %d not greater than 0",
                                        layout_struct->name, counter.u.i);
        }
        layout_struct->counter = counter.u.i;
    }

    toml_datum_t ignore_excluded_regions = toml_bool_in(layout_table, "ignore-excluded-regions");
    if (!ignore_excluded_regions.ok) {
        layout_struct->ignore_excluded_regions = false;
    } else {
        layout_struct->ignore_excluded_regions = ignore_excluded_regions.u.b;
    }

    toml_datum_t no_borders = toml_bool_in(layout_table, "no-borders");
    if (!no_borders.ok) {
        layout_struct->no_borders = false;
    } else {
        layout_struct->no_borders = no_borders.u.b;
    }

    wlr_log(WLR_DEBUG, "Parsed [[layout]] with name \"%s\", layout function \"%s\", initial parameter %f, initial counter %d, ignore excluded regions %d, no borders %d",
            layout_struct->name, layout.u.s, layout_struct->parameter, layout_struct->counter,
            layout_struct->ignore_excluded_regions, layout_struct->no_borders);

    free(layout_name);
    free(name.u.s);
}

static void parse_libinput_config_table(toml_table_t *libinput_table, struct viv_libinput_config *libinput_config) {
    toml_datum_t device_name = toml_string_in(libinput_table, "device-name");
    if (!device_name.ok) {
        EXIT_WITH_MESSAGE("Error parsing [[libinput-config]]: no \"device-name\" provided");
    }

    toml_datum_t scroll_method = toml_string_in(libinput_table, "scroll-method");
    if (scroll_method.ok) {
        enum libinput_config_scroll_method scroll_method_enum;
        bool success = look_up_string_in_map(scroll_method.u.s, scroll_method_map, &scroll_method_enum);
        if (!success) {
            EXIT_WITH_FORMATTED_MESSAGE("Error parsing [[libinput-config]] for device \"%s\": scroll-method \"%s\" not recognised",
                                        device_name.u.s, scroll_method.u.s);
        } else {
            libinput_config->scroll_method = scroll_method_enum;
        }
        free(scroll_method.u.s);
    }

    toml_datum_t scroll_button = toml_int_in(libinput_table, "scroll-button");
    if (scroll_button.ok) {
        libinput_config->scroll_button = (uint32_t)scroll_button.u.i;
    }

    toml_datum_t middle_emulation = toml_bool_in(libinput_table, "middle-emulation");
    if (middle_emulation.ok) {
        libinput_config->middle_emulation = (uint32_t)middle_emulation.u.i;
    }

    toml_datum_t left_handed = toml_bool_in(libinput_table, "left-handed");
    if (left_handed.ok) {
        libinput_config->left_handed = (uint32_t)left_handed.u.i;
    }

    toml_datum_t natural_scroll = toml_bool_in(libinput_table, "natural-scroll");
    if (natural_scroll.ok) {
        libinput_config->natural_scroll = (uint32_t)natural_scroll.u.i;
    }

    wlr_log(WLR_DEBUG, "Parsed [[libinput-config]] for device \"%s\", scroll method %d, scroll button %d, middle emulation %d, left handed %d, natural scroll %d",
            device_name.u.s, libinput_config->scroll_method, libinput_config->scroll_button,
            libinput_config->middle_emulation, libinput_config->left_handed, libinput_config->natural_scroll);

    libinput_config->device_name = device_name.u.s;
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
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "keybind", struct viv_keybind, parse_keybind_table, &config->keybinds);

    // [[layout]] list
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "layout", struct viv_layout, parse_layout_table, &config->layouts);
    /* parse_config_array_variable_length(root, "layout", NULL); */
    /* parse_config_array_variable_length(root, "layout", parse_layout); */

    // [[libinput-config]] list
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "libinput-config", struct viv_libinput_config, parse_libinput_config_table, &config->libinput_configs);
    /* parse_config_array_variable_length(root, "keybind", NULL); */
    /* parse_config_array_variable_length(root, "keybind", parse_libinput_config); */

    toml_free(root);

    EXIT_WITH_MESSAGE("done");

    return config;
}
