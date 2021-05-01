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
#include "viv_mappable_functions.h"
#include "viv_toml_config.h"
#include "viv_types.h"

#define ERRBUF_SIZE 200

struct string_map_pair {
    char *key;
    uint32_t value;
};

#define NULL_STRING_MAP_PAIR {"", 0}
#define IS_NULL_STRING_MAP_PAIR(ROW) (strlen(ROW) == 0)
#define MAX_MAP_LEN 1000

#define GLOBAL_META_STRING "meta"

#define PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(ROOT, SECTION_NAME, TYPE, PARSE_FUNC, ARRAY_TERMINATOR, TARGET) \
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
        CHECK_ALLOCATION(the_array);                                    \
                                                                            \
        for (size_t i = 0; i < our_array_nelem - 1; i++) {                  \
            PARSE_FUNC(toml_table_at(array, i), &the_array[i]);          \
        }                                                                   \
                                                                        \
        TYPE terminator = ARRAY_TERMINATOR;                             \
        memcpy(&the_array[our_array_nelem - 1], &terminator, sizeof(TYPE)); \
                                                                        \
        *TARGET = the_array;                                            \
    } while (false);

#define CHECK_MAPPABLE_ACTION_STRING(FUNCTION_NAME, DOC, ...)            \
    if (strcmp(action, #FUNCTION_NAME) == 0) {                          \
        *mappable = &viv_mappable_ ## FUNCTION_NAME;                    \
        result = true;                                                  \
    }
static bool action_string_to_mappable(char *action, void (**mappable)(struct viv_workspace *workspace, union viv_mappable_payload payload)) {
    bool result = false;
    MACRO_FOR_EACH_MAPPABLE(CHECK_MAPPABLE_ACTION_STRING);
    return result;
}

static struct string_map_pair meta_map[] = {
    {GLOBAL_META_STRING, 0u},  // note 0u is replaced with the user's meta choice at runtime
    {"shift", WLR_MODIFIER_SHIFT},
    {"caps", WLR_MODIFIER_CAPS},
    {"ctrl", WLR_MODIFIER_CTRL},
    {"alt", WLR_MODIFIER_ALT},
    {"mod2", WLR_MODIFIER_MOD2},
    {"mod3", WLR_MODIFIER_MOD3},
    {"logo", WLR_MODIFIER_LOGO},
    {"mod5", WLR_MODIFIER_MOD5},
    NULL_STRING_MAP_PAIR,
};

static struct string_map_pair cursor_button_map[] = {
    {"left", VIV_LEFT_BUTTON},
    {"right", VIV_RIGHT_BUTTON},
    {"middle", VIV_MIDDLE_BUTTON},
    NULL_STRING_MAP_PAIR,
};

static struct string_map_pair scroll_method_map[] = {
    {"no-scroll", LIBINPUT_CONFIG_SCROLL_NO_SCROLL},
    {"2-finger", LIBINPUT_CONFIG_SCROLL_2FG},
    {"edge", LIBINPUT_CONFIG_SCROLL_EDGE},
    {"on-button-down", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN},
    {"scroll-button", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN},
    NULL_STRING_MAP_PAIR,
};

static struct string_map_pair damage_tracking_mode_map[] = {
    {"none", VIV_DAMAGE_TRACKING_NONE},
    {"frame", VIV_DAMAGE_TRACKING_FRAME},
    {"full", VIV_DAMAGE_TRACKING_FULL},
    NULL_STRING_MAP_PAIR,
};

static bool is_null_string_map_pair(struct string_map_pair *row) {
    return (strlen(row->key) == 0);
}

static bool look_up_string_in_map(char *string, struct string_map_pair *map, uint32_t *result) {
    for (size_t i = 0; i < MAX_MAP_LEN; i++) {
        struct string_map_pair row = map[i];
        if (is_null_string_map_pair(&row)) {
            break;
        }

        if (strcmp(string, row.key) == 0) {
            *result = row.value;
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
    wlr_log(WLR_DEBUG, "Parsed %s.%s = %d as bool", section_name, key_name, *target);
}

static void parse_config_int(toml_table_t *root, char *section_name, char *key_name, int *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_int_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as int", section_name, key_name);
    }
    *target = key.u.i;
    wlr_log(WLR_DEBUG, "Parsed %s.%s = %d as int", section_name, key_name, *target);
}

static void parse_config_uint(toml_table_t *root, char *section_name, char *key_name, uint32_t *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_int_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as int", section_name, key_name);
    }
    if (!(key.u.i >= 0)) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as unsigned int, got %d",
                                    section_name, key_name, (int)key.u.i);
    }
    *target = (uint32_t)key.u.i;
    wlr_log(WLR_DEBUG, "Parsed %s.%s = %d as int", section_name, key_name, *target);
}

static void parse_config_double(toml_table_t *root, char *section_name, char *key_name, double *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_double_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as double", section_name, key_name);
    }
    *target = key.u.d;
    wlr_log(WLR_DEBUG, "Parsed %s.%s = %f as double", section_name, key_name, *target);
}

static void parse_config_string_map(toml_table_t *root, char *section_name, char *key_name, struct string_map_pair *map, uint32_t *target) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_string_in(section, key_name);
    if (!key.ok) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as string", section_name, key_name);
    }
    wlr_log(WLR_DEBUG, "Parsed %s.%s = \"%s\" as string", section_name, key_name, key.u.s);

    bool success = look_up_string_in_map(key.u.s, map, target);
    if (!success) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s: value \"%s\" not valid",
                                    section_name, key_name, key.u.s);
    }
    free(key.u.s);
}

static void parse_config_string_raw(toml_table_t *root, char *section_name, char *key_name, char **target, bool allow_missing) {
    toml_table_t *section = toml_table_in(root, section_name);
    toml_datum_t key = toml_string_in(section, key_name);
    if (key.ok) {
        *target = key.u.s;
        wlr_log(WLR_DEBUG, "Parsed %s.%s = \"%s\" as string", section_name, key_name, *target);
    } else {
        if (allow_missing) {
            char *default_value = (*target) ? (*target) : "NULL";
            wlr_log(WLR_DEBUG, "No key found for  %s.%s, using default \"%s\"", section_name, key_name, default_value);
        } else {
            EXIT_WITH_FORMATTED_MESSAGE("Config error reading %s.%s as string", section_name, key_name);
        }
    }

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
        wlr_log(WLR_DEBUG, "Parsed %s.%s[%d] = %f as float", section_name, key_name, (uint32_t)i, target[i]);
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
    } else if (strcmp(action, "increment_counter") == 0) {
        toml_datum_t increment_int = toml_int_in(table, "increment");
        if (!increment_int.ok) {
            EXIT_WITH_FORMATTED_MESSAGE("Config error parsing [[keybind]] for action %s: missing \"increment\" key",
                                        action);
        }
        payload->increment_divide.increment = increment_int.u.d;
        wlr_log(WLR_DEBUG, "Parsed argument \"%s\" = %f for action \"%s\"",
                "increment", increment_int.u.d, action);
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

    keybind->type = VIV_KEYBIND_TYPE_KEYSYM;
    keybind->key = key;
    keybind->modifiers = modifiers_bitfield;

    bool action_recognised = action_string_to_mappable(action.u.s, &keybind->binding);
    if (!action_recognised) {
        EXIT_WITH_FORMATTED_MESSAGE("Config error reading [[keybind]] for keysym %s: action \"%s\" not recognised",
                                    keysym.u.s, action.u.s);
    }

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
                                        layout_struct->name, (int)counter.u.i);
        }
        layout_struct->counter = counter.u.i;
    }

    toml_datum_t ignore_excluded_regions = toml_bool_in(layout_table, "ignore-excluded-regions");
    if (!ignore_excluded_regions.ok) {
        layout_struct->ignore_excluded_regions = false;
    } else {
        layout_struct->ignore_excluded_regions = ignore_excluded_regions.u.b;
    }

    toml_datum_t show_borders = toml_bool_in(layout_table, "show-borders");
    if (!show_borders.ok) {
        layout_struct->no_borders = false;
    } else {
        layout_struct->no_borders = !show_borders.u.b;
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

/// Allocate and return a new keybinds list with autogenerated bindings for switching
/// workspace and moving windows to each workspace. The autogenerated bindings use the
/// number row.
static struct viv_keybind *append_workspace_keybinds(char workspaces[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LENGTH], struct viv_keybind *keybinds, uint32_t global_meta_key) {
    uint32_t num_workspaces = 0;
    for (size_t i = 0; i < MAX_NUM_WORKSPACES; i++) {
        if (strlen(workspaces[i]) == 0) {
            num_workspaces = (uint32_t)i;
            break;
        }
    }

    uint32_t old_num_keybinds = 0;
    for (size_t i = 0; i < MAX_NUM_KEYBINDS; i++) {
        if (keybinds[i].key == NULL_KEY) {
            old_num_keybinds = (uint32_t)i;
            break;
        }
    }

    if (num_workspaces > 10) {
        // Only 10 number keys to use
        wlr_log(WLR_ERROR,
                "Tried to autogenerate bindings for %d workspaces, but cannot as "
                "there are only 10 number keys. Binding first 10 only.", num_workspaces);
        num_workspaces = 10;
    }

    // one keybind for switch-to-workspace, one for move-active-window
    // + 1 for null keybind terminator
    uint32_t additional_num_keybinds = num_workspaces * 2;
    uint32_t new_num_keybinds = old_num_keybinds + additional_num_keybinds + 1;

    struct viv_keybind *new_keybinds = calloc(new_num_keybinds + 1,
                                              sizeof(struct viv_keybind));
    CHECK_ALLOCATION(new_keybinds);

    // Copy existing keybinds into place
    for (size_t i = 0; i < old_num_keybinds; i++) {
        memcpy(&new_keybinds[i], &keybinds[i], sizeof(struct viv_keybind));
    }

    // Make new keybinds
    // Bind switch to workspace
    for (size_t i = 0; i < num_workspaces; i++) {
        struct viv_keybind *keybind = &new_keybinds[old_num_keybinds + i];
        keybind->type = VIV_KEYBIND_TYPE_KEYCODE;
        keybind->modifiers = global_meta_key;
        keybind->keycode = 10 + i;  // 10 is keycode for number 1 - TODO: find proper source for this
        keybind->binding = &viv_mappable_switch_to_workspace;
        strcpy(keybind->payload.switch_to_workspace.workspace_name, workspaces[i]);
    }

    // Bind move active window to workspace
    for (size_t i = 0; i < num_workspaces; i++) {
        struct viv_keybind *keybind = &new_keybinds[old_num_keybinds + num_workspaces + i];
        keybind->type = VIV_KEYBIND_TYPE_KEYCODE;
        keybind->modifiers = global_meta_key | WLR_MODIFIER_SHIFT;
        keybind->keycode = 10 + i;  // 10 is keycode for number 1 - TODO: find proper source for this
        keybind->binding = &viv_mappable_shift_active_window_to_workspace;
        strcpy(keybind->payload.switch_to_workspace.workspace_name, workspaces[i]);
    }

    struct viv_keybind null_keybind = TERMINATE_KEYBINDS_LIST();
    memcpy(&new_keybinds[old_num_keybinds + additional_num_keybinds + 1], &null_keybind, sizeof(struct viv_keybind));

    return new_keybinds;
}

void load_file_as_toml_config(FILE *fp, struct viv_config *config) {
    UNUSED(parse_config_double);

    char errbuf[ERRBUF_SIZE];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));

    if (!root) {
        wlr_log(WLR_ERROR, "Could not parse config.toml:");
        EXIT_WITH_MESSAGE(errbuf);
    }

    toml_table_t *global_config = toml_table_in(root, "global-config");
    if (!global_config) {
        EXIT_WITH_MESSAGE("Config error: no [global-config] section found but this section is compulsory");
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
    parse_config_array_fixed_size_float_float(root, "global-config", "clear-colour", 4, config->clear_colour);

    toml_array_t *workspace_names = toml_array_in(global_config, "workspace-names");
    if (toml_array_kind(workspace_names) != 'v') {
        EXIT_WITH_FORMATTED_MESSAGE("Config error parsing workspace-names: expected array of type 'v', got type %c",
                                    toml_array_kind(workspace_names));
    }
    uint32_t our_array_nelem = toml_array_nelem(workspace_names);
    for (size_t i = 0; i < our_array_nelem; i++) {
        toml_datum_t name = toml_string_at(workspace_names, i);
        strncpy((char *)&config->workspaces[i], name.u.s, MAX_WORKSPACE_NAME_LENGTH);
        wlr_log(WLR_DEBUG, "Parsed workspace name \"%s\"", config->workspaces[i]);
    }

    // [background]
    parse_config_string_raw(root, "background", "colour", &config->background.colour, true);
    parse_config_string_raw(root, "background", "image", &config->background.image, true);
    parse_config_string_raw(root, "background", "mode", &config->background.mode, true);

    // [xkb-config]
    parse_config_string_raw(root, "xkb-config", "rules", &config->xkb_rules.rules, true);
    parse_config_string_raw(root, "xkb-config", "model", &config->xkb_rules.model, true);
    parse_config_string_raw(root, "xkb-config", "layout", &config->xkb_rules.layout, true);
    parse_config_string_raw(root, "xkb-config", "variant", &config->xkb_rules.variant, true);
    parse_config_string_raw(root, "xkb-config", "options", &config->xkb_rules.options, true);

    // [ipc]
    parse_config_string_raw(root, "ipc", "workspaces-filename", &config->ipc_workspaces_filename, true);

    // [bar]
    parse_config_string_raw(root, "bar", "command", &config->bar.command, true);
    parse_config_uint(root, "bar", "update-signal-number", &config->bar.update_signal_number);

    // [debug]
    parse_config_bool(root, "debug", "mark-views-by-shell", &config->debug_mark_views_by_shell);
    parse_config_bool(root, "debug", "mark-active-output", &config->debug_mark_active_output);
    parse_config_bool(root, "debug", "draw-only-damaged-regions", &config->debug_draw_only_damaged_regions);
    parse_config_bool(root, "debug", "mark-frame-draws", &config->debug_mark_frame_draws);
    parse_config_string_map(root, "debug", "damage-tracking-mode", damage_tracking_mode_map,
                            &config->damage_tracking_mode);

    // [[keybind]] list
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "keybind", struct viv_keybind, parse_keybind_table, TERMINATE_KEYBINDS_LIST(), &config->keybinds);
    bool bind_workspaces_to_numbers = false;
    parse_config_bool(root, "global-config", "bind-workspaces-to-numbers", &bind_workspaces_to_numbers);
    if (bind_workspaces_to_numbers) {
        struct viv_keybind *orig_keybinds = config->keybinds;
        config->keybinds = append_workspace_keybinds(config->workspaces, config->keybinds, config->global_meta_key);
        free(orig_keybinds);
    }


    // [[layout]] list
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "layout", struct viv_layout, parse_layout_table, TERMINATE_LAYOUTS_LIST(), &config->layouts);

    // [[libinput-config]] list
    PARSE_CONFIG_ARRAY_VARIABLE_LENGTH(root, "libinput-config", struct viv_libinput_config, parse_libinput_config_table, TERMINATE_LIBINPUT_CONFIG_LIST(), &config->libinput_configs);

    toml_free(root);
}

void viv_toml_config_load(char *path, struct viv_config *config, bool user_path) {
    wlr_log(WLR_INFO, "Loading toml config at path \"%s\"", path);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (user_path) {
            EXIT_WITH_FORMATTED_MESSAGE("Could not open user-specified config at \"%s\"", path);
        } else {
            // We didn't find the config in a default location, that's fine
            wlr_log(WLR_INFO, "Config not found at default path \"%s\", skipping config load", path);
        }
    } else
    {
        load_file_as_toml_config(fp, config);
        fclose(fp);
    }
}
