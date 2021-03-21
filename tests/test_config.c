#include <fff.h>
#include <unity.h>

#include "viv_config.h"
#include "viv_config_support.h"
#include "viv_toml_config.h"

DEFINE_FFF_GLOBALS;
#include "mock_layouts.h"
#include "mock_mappable_functions.h"

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
void test_config_toml_matches_defaults(void) {
    struct viv_config load_config = { 0 };
    viv_toml_config_load(default_config_path, &load_config, true);

    struct viv_config default_config = the_config;

    TEST_ASSERT_EQUAL(default_config.global_meta_key, load_config.global_meta_key);
    TEST_ASSERT_EQUAL(default_config.focus_follows_mouse, load_config.focus_follows_mouse);
    TEST_ASSERT_EQUAL(default_config.win_move_cursor_button, load_config.win_move_cursor_button);
    TEST_ASSERT_EQUAL(default_config.win_resize_cursor_button, load_config.win_resize_cursor_button);

    TEST_ASSERT_EQUAL(default_config.border_width, load_config.border_width);

    TEST_ASSERT_EQUAL_FLOAT_ARRAY(default_config.active_border_colour, load_config.active_border_colour, RGBA_NUM_VALUES);
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(default_config.inactive_border_colour, load_config.inactive_border_colour, RGBA_NUM_VALUES);

    TEST_ASSERT_EQUAL(default_config.gap_width, load_config.gap_width);

    TEST_ASSERT_EQUAL_FLOAT_ARRAY(default_config.clear_colour, load_config.clear_colour, RGBA_NUM_VALUES);

    TEST_ASSERT_EQUAL_STRING(default_config.background.colour, load_config.background.colour);
    TEST_ASSERT_EQUAL_STRING(default_config.background.image, load_config.background.image);
    TEST_ASSERT_EQUAL_STRING(default_config.background.mode, load_config.background.mode);

    TEST_ASSERT_EQUAL_STRING(default_config.xkb_rules.rules, load_config.xkb_rules.rules);
    TEST_ASSERT_EQUAL_STRING(default_config.xkb_rules.model, load_config.xkb_rules.model);
    TEST_ASSERT_EQUAL_STRING(default_config.xkb_rules.layout, load_config.xkb_rules.layout);
    TEST_ASSERT_EQUAL_STRING(default_config.xkb_rules.variant, load_config.xkb_rules.variant);
    TEST_ASSERT_EQUAL_STRING(default_config.xkb_rules.options, load_config.xkb_rules.options);

    TEST_ASSERT_EQUAL_STRING(default_config.ipc_workspaces_filename, load_config.ipc_workspaces_filename);

    TEST_ASSERT_EQUAL_STRING(default_config.bar.command, load_config.bar.command);
    TEST_ASSERT_EQUAL(default_config.bar.update_signal_number, load_config.bar.update_signal_number);

    TEST_ASSERT_EQUAL(default_config.debug_mark_views_by_shell, load_config.debug_mark_views_by_shell);
    TEST_ASSERT_EQUAL(default_config.debug_mark_active_output, load_config.debug_mark_active_output);

    // Keybinds

}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    UNITY_BEGIN();
    RUN_TEST(test_config_load);
    RUN_TEST(test_config_toml_matches_defaults);
    return UNITY_END();
}
