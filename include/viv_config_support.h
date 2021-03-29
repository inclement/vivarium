#ifndef VIV_CONFIG_SUPPORT_H
#define VIV_CONFIG_SUPPORT_H

#define NULL_KEY 0
#define TERMINATE_KEYBINDS_LIST() { .key = NULL_KEY }
#define TERMINATE_LAYOUTS_LIST() { .name = "" }
#define TERMINATE_LIBINPUT_CONFIG_LIST() { .device_name = "" }

#define MAX_NUM_KEYBINDS 10000
#define MAX_NUM_LIBINPUT_CONFIGS 10000
#define MAX_WORKSPACE_NAME_LENGTH 80
#define MAX_NUM_WORKSPACES 50
#define MAX_NUM_LAYOUTS 50

#include <stdlib.h>

#include <wlr/util/log.h>

#define NO_MODIFIERS (0u)

#define KEYBIND_MAPPABLE(MODIFIERS, KEY, BINDING, ...)   \
    {                                           \
        .type = VIV_KEYBIND_TYPE_KEYSYM,        \
        .key = XKB_KEY_ ## KEY,                 \
        .modifiers = (MODIFIERS),               \
        .binding = &viv_mappable_ ## BINDING,   \
        .payload = { .BINDING = { ._empty = 0u, __VA_ARGS__ } }  \
    }

#define KEYBIND_USER_FUNCTION(MODIFIERS, KEY, BINDING)           \
    {                                                            \
        .type = VIV_KEYBIND_TYPE_KEYSYM,                         \
        .key = XKB_KEY_ ## KEY,                                  \
        .modifiers = (MODIFIERS),                                \
        .binding = &viv_mappable_user_function,                  \
        .payload = { .user_function = { .function = BINDING } }  \
    }

#define EXIT_WITH_MESSAGE(MESSAGE)             \
    wlr_log(WLR_ERROR, "%s", MESSAGE);          \
    exit(EXIT_FAILURE);

#define EXIT_WITH_FORMATTED_MESSAGE(MESSAGE, ...)   \
    wlr_log(WLR_ERROR, MESSAGE, __VA_ARGS__);                   \
    exit(EXIT_FAILURE);

#define CHECK_ALLOCATION(POINTER)                           \
    if (!POINTER) {                                         \
        EXIT_WITH_MESSAGE("ALLOCATION FAILURE: " #POINTER); \
    }

#define ASSERT(EXPR)                                \
    if (!(EXPR)) {                                  \
        EXIT_WITH_MESSAGE("Assert failed: " #EXPR); \
    }

#define UNREACHABLE() \
    EXIT_WITH_MESSAGE("UNREACHABLE");

#define UNUSED(SYMBOL) \
    (void)(SYMBOL);

#endif
