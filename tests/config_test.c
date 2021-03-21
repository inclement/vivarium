#include <fff.h>
#include <unity.h>

#include "viv_config.h"
#include "viv_config_support.h"
/* #include "viv_toml_config.h" */

DEFINE_FFF_GLOBALS;
#include "mock_layouts.h"
#include "mock_mappable_functions.h"

FAKE_VOID_FUNC(viv_view_set_target_box, struct viv_view *, uint32_t, uint32_t, uint32_t, uint32_t);

void setUp() {
}

void tearDown() {
}

void test_unity_test(void) {
    TEST_ASSERT_EQUAL(0, 0);
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    UNUSED(the_config);

    UNITY_BEGIN();
    RUN_TEST(test_unity_test);
    return UNITY_END();
}
