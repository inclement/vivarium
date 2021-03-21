#include <fff.h>
#include <unity.h>

#include "viv_config.h"
#include "viv_config_support.h"
#include "viv_toml_config.h"

DEFINE_FFF_GLOBALS;
#include "mock_layouts.h"
#include "mock_mappable_functions.h"

// Arbitrary number intended to be higher than anything realistic
#define MAX_LEN_STATIC_LISTS 1000

FAKE_VOID_FUNC(viv_view_set_target_box, struct viv_view *, uint32_t, uint32_t, uint32_t, uint32_t);

#define RGBA_NUM_VALUES 4

static char *default_config_path = "../config/config.toml";

void setUp() {
    RESET_LAYOUT_FAKES();
    RESET_MAPPABLE_FUNCTION_FAKES();
}

void tearDown() {
}

/// The config.toml can be loaded/parsed without crashing
void test_config_load(void) {
    struct viv_config config = { 0 };
    viv_toml_config_load(default_config_path, &config, true);
}

/// The config.toml content matches the compiled-in defaults - this is important so that a
/// user merely copying the config into place doesn't change Vivarium's behaviour.
#define TEST_ASSERT_CONFIG_EQUAL(KEY) TEST_ASSERT_EQUAL_MESSAGE(default_config.KEY, load_config.KEY, "Mismatching key = " #KEY)
#define TEST_ASSERT_CONFIG_EQUAL_STRING(KEY) TEST_ASSERT_EQUAL_STRING_MESSAGE(default_config.KEY, load_config.KEY, "Mismatching key = " #KEY)
#define TEST_ASSERT_CONFIG_EQUAL_FLOAT(KEY) TEST_ASSERT_EQUAL_FLOAT_MESSAGE(default_config.KEY, load_config.KEY, "Mismatching key = " #KEY)
#define TEST_ASSERT_CONFIG_EQUAL_FLOAT_ARRAY(KEY, LEN) TEST_ASSERT_EQUAL_FLOAT_ARRAY_MESSAGE(default_config.KEY, load_config.KEY, LEN, "Mismatching key = " #KEY)
void test_config_toml_static_options_match_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);
    struct viv_config default_config = the_config;

    TEST_ASSERT_CONFIG_EQUAL(global_meta_key);
    TEST_ASSERT_CONFIG_EQUAL(focus_follows_mouse);
    TEST_ASSERT_CONFIG_EQUAL(win_move_cursor_button);
    TEST_ASSERT_CONFIG_EQUAL(win_resize_cursor_button);

    TEST_ASSERT_CONFIG_EQUAL(border_width);

    TEST_ASSERT_CONFIG_EQUAL_FLOAT_ARRAY(active_border_colour, RGBA_NUM_VALUES);
    TEST_ASSERT_CONFIG_EQUAL_FLOAT_ARRAY(inactive_border_colour, RGBA_NUM_VALUES);

    TEST_ASSERT_CONFIG_EQUAL(gap_width);

    TEST_ASSERT_CONFIG_EQUAL_FLOAT_ARRAY(clear_colour, RGBA_NUM_VALUES);

    TEST_ASSERT_CONFIG_EQUAL_STRING(background.colour);
    TEST_ASSERT_CONFIG_EQUAL_STRING(background.image);
    TEST_ASSERT_CONFIG_EQUAL_STRING(background.mode);

    TEST_ASSERT_CONFIG_EQUAL_STRING(xkb_rules.rules);
    TEST_ASSERT_CONFIG_EQUAL_STRING(xkb_rules.model);
    TEST_ASSERT_CONFIG_EQUAL_STRING(xkb_rules.layout);
    TEST_ASSERT_CONFIG_EQUAL_STRING(xkb_rules.variant);
    TEST_ASSERT_CONFIG_EQUAL_STRING(xkb_rules.options);

    TEST_ASSERT_CONFIG_EQUAL_STRING(ipc_workspaces_filename);

    TEST_ASSERT_CONFIG_EQUAL_STRING(bar.command);
    TEST_ASSERT_CONFIG_EQUAL(bar.update_signal_number);

    TEST_ASSERT_CONFIG_EQUAL(debug_mark_views_by_shell);
    TEST_ASSERT_CONFIG_EQUAL(debug_mark_active_output);
}

void test_config_toml_workspaces_match_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);
    struct viv_config default_config = the_config;

    for (size_t i = 0; i < MAX_LEN_STATIC_LISTS; i++) {
        char *default_name = default_config.workspaces[i];
        char *load_name = load_config.workspaces[i];

        if (!strlen(default_name)) {
            break;
        }

        TEST_ASSERT_EQUAL_STRING(default_name, load_name);

    }
}

void test_config_toml_keybinds_match_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);
    struct viv_config default_config = the_config;

    for (size_t i = 0; i < MAX_LEN_STATIC_LISTS; i++) {
        struct viv_keybind default_keybind = default_config.keybinds[i];
        struct viv_keybind load_keybind = load_config.keybinds[i];

        if (default_keybind.binding == &viv_mappable_user_function) {
            // The user function binding marks the end of the default config needing to
            // match config.toml
            break;
        }

        TEST_ASSERT_EQUAL_HEX(default_keybind.key, load_keybind.key);
        TEST_ASSERT_EQUAL(default_keybind.modifiers, load_keybind.modifiers);
        TEST_ASSERT_EQUAL(default_keybind.binding, load_keybind.binding);

        if (default_keybind.key == NULL_KEY) {
            break;
        }
    }
}

void test_config_toml_libinput_configs_match_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);
    struct viv_config default_config = the_config;

    for (size_t i = 0; i < MAX_LEN_STATIC_LISTS; i++) {
        struct viv_libinput_config default_li_config = default_config.libinput_configs[i];
        struct viv_libinput_config load_li_config = load_config.libinput_configs[i];

        TEST_ASSERT_EQUAL_STRING(default_li_config.device_name, load_li_config.device_name);
        TEST_ASSERT_EQUAL(default_li_config.scroll_method, load_li_config.scroll_method);
        TEST_ASSERT_EQUAL(default_li_config.scroll_button, load_li_config.scroll_button);
        TEST_ASSERT_EQUAL(default_li_config.middle_emulation, load_li_config.middle_emulation);
        TEST_ASSERT_EQUAL(default_li_config.left_handed, load_li_config.left_handed);
        TEST_ASSERT_EQUAL(default_li_config.natural_scroll, load_li_config.natural_scroll);

        if (strlen(default_li_config.device_name) == 0) {
            break;
        }
    }
}

void test_config_toml_layouts_match_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);
    struct viv_config default_config = the_config;

    for (size_t i = 0; i < MAX_LEN_STATIC_LISTS; i++) {
        struct viv_layout default_layout = default_config.layouts[i];
        struct viv_layout load_layout = load_config.layouts[i];

        if (strcmp(default_layout.name, "User defined layout") == 0) {
            break;
        }

        TEST_ASSERT_EQUAL_STRING(default_layout.name, load_layout.name);
        TEST_ASSERT_EQUAL(default_layout.layout_function, load_layout.layout_function);
        TEST_ASSERT_EQUAL(default_layout.parameter, load_layout.parameter);
        TEST_ASSERT_EQUAL(default_layout.counter, load_layout.counter);
        TEST_ASSERT_EQUAL(default_layout.no_borders, load_layout.no_borders);
        TEST_ASSERT_EQUAL(default_layout.ignore_excluded_regions, load_layout.ignore_excluded_regions);

        if (strlen(default_layout.name) == 0) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    UNITY_BEGIN();
    RUN_TEST(test_config_load);
    RUN_TEST(test_config_toml_static_options_match_defaults);
    RUN_TEST(test_config_toml_workspaces_match_defaults);
    RUN_TEST(test_config_toml_keybinds_match_defaults);
    RUN_TEST(test_config_toml_libinput_configs_match_defaults);
    RUN_TEST(test_config_toml_layouts_match_defaults);
    return UNITY_END();
}
