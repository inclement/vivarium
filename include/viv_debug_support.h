
#ifdef DEBUG
#define DEBUG_ASSERT_EQUAL(EXPR1, EXPR2)                                \
    if ((EXPR1) != (EXPR2)) {                                           \
        wlr_log(WLR_ERROR, "ERROR IN DEBUG ASSERT: (" #EXPR1 ") != (" #EXPR2 ")"); \
    }
#define DEBUG_ASSERT(EXPR)                                 \
    if (!(EXPR)) {                                         \
        wlr_log(WLR_ERROR, "DEBUG ASSERT FAILURE: failed ASSERT(" #EXPR ")"); \
    }
#else
#define DEBUG_ASSERT_EQUAL(EXPR1, EXPR2)
#define DEBUG_ASSERT(EXPR)
#endif
