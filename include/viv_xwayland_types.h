#ifndef VIV_XWAYLAND_TYPES_H
#define VIV_XWAYLAND_TYPES_H

/// See documentation at https://specifications.freedesktop.org/wm-spec/1.4/ar01s05.html
#define MACRO_FOR_EACH_ATOM_NAME(MACRO)                          \
    MACRO(_NET_WM_WINDOW_TYPE_DESKTOP)                           \
    MACRO(_NET_WM_WINDOW_TYPE_DOCK)                              \
    MACRO(_NET_WM_WINDOW_TYPE_TOOLBAR)                           \
    MACRO(_NET_WM_WINDOW_TYPE_MENU)                              \
    MACRO(_NET_WM_WINDOW_TYPE_UTILITY)                           \
    MACRO(_NET_WM_WINDOW_TYPE_SPLASH)                            \
    MACRO(_NET_WM_WINDOW_TYPE_DIALOG)                            \
    MACRO(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU)                     \
    MACRO(_NET_WM_WINDOW_TYPE_POPUP_MENU)                        \
    MACRO(_NET_WM_WINDOW_TYPE_TOOLTIP)                           \
    MACRO(_NET_WM_WINDOW_TYPE_NOTIFICATION)                      \
    MACRO(_NET_WM_WINDOW_TYPE_COMBO)                             \
    MACRO(_NET_WM_WINDOW_TYPE_DND)                               \
    MACRO(_NET_WM_WINDOW_TYPE_NORMAL)                            \

#define AS_SYMBOL(ATOM) ATOM,
enum window_type_atom {
    MACRO_FOR_EACH_ATOM_NAME(AS_SYMBOL)
    WINDOW_TYPE_ATOM_MAX,
};
#undef AS_SYMBOL

#endif
