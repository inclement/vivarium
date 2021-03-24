#include <fff.h>
#include <unity.h>
#include <wayland-util.h>

#include "viv_layout.h"
#include "viv_config_support.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(viv_view_set_target_box, struct viv_view *, uint32_t, uint32_t, uint32_t, uint32_t);

void setUp() {
    RESET_FAKE(viv_view_set_target_box);
    FFF_RESET_HISTORY();
}

void tearDown() {
}

static void wl_array_append_view(struct wl_array *array, struct viv_view *view) {
    *(struct viv_view **)wl_array_add(array, sizeof(struct viv_view *)) = view;
}

#define DEFAULT_NUM_VIEWS 5
#define DEFAULT_FLOAT_PARAM 0.66
#define DEFAULT_COUNTER_PARAM 1
#define DEFAULT_WIDTH 1024
#define DEFAULT_HEIGHT 768
#define MACRO_FOR_EACH_TEST_CASE(MACRO, LAYOUT_NAME)                                \
    MACRO(LAYOUT_NAME, 0, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, 1, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, 2, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, 3, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, 4, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, 5, DEFAULT_FLOAT_PARAM, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, 0, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, 0.5, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, 1, DEFAULT_COUNTER_PARAM, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, DEFAULT_FLOAT_PARAM, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, DEFAULT_FLOAT_PARAM, 1, DEFAULT_WIDTH, DEFAULT_HEIGHT) \
    MACRO(LAYOUT_NAME, DEFAULT_NUM_VIEWS, DEFAULT_FLOAT_PARAM, 2, DEFAULT_WIDTH, DEFAULT_HEIGHT) \

/// Call the given layout func with the given parameters, asserting only that it doesn't
/// crash and that it calls viv_view_set_target_box the right number of times
void do_test(void (layout_func)(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height), uint32_t num_views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    struct wl_array views;
    wl_array_init(&views);

    struct viv_view view;

    for (size_t i = 0; i < num_views; i++) {
        wl_array_append_view(&views, &view);
    }
    layout_func(&views, float_param, counter_param, width, height);
    TEST_ASSERT_EQUAL(num_views, viv_view_set_target_box_fake.call_count);
    RESET_FAKE(viv_view_set_target_box);
}

#define DO_TEST(LAYOUT_NAME, NUM_VIEWS, FLOAT_PARAM, COUNTER_PARAM, WIDTH, HEIGHT) \
    do_test(viv_layout_do_ ## LAYOUT_NAME, NUM_VIEWS, FLOAT_PARAM, COUNTER_PARAM, WIDTH, HEIGHT); \

#define GENERATE_TEST_CASE_NAME(LAYOUT_NAME) test_layout_ ## LAYOUT_NAME ## _with_various_params
#define GENERATE_TEST_CASE(LAYOUT_NAME, _1, _2)                            \
    void GENERATE_TEST_CASE_NAME(LAYOUT_NAME)(void) {                   \
        MACRO_FOR_EACH_TEST_CASE(DO_TEST, LAYOUT_NAME);                             \
    }                                                           \

#define GENERATE_TEST_RUN(LAYOUT_NAME, _1, _2) RUN_TEST(GENERATE_TEST_CASE_NAME(LAYOUT_NAME));

MACRO_FOR_EACH_LAYOUT(GENERATE_TEST_CASE)

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    UNITY_BEGIN();
    MACRO_FOR_EACH_LAYOUT(GENERATE_TEST_RUN);
    return UNITY_END();
}
