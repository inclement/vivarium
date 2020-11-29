
#include <wayland-util.h>

#include "viv_config_support.h"

struct wl_list *viv_wl_list_next_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element) {
    // This can only be safely called if the list is guaranteed to have another element to find
    ASSERT(wl_list_length(root_element) > 1);

    struct wl_list *next_element = cur_element->next;
    if (next_element == root_element) {
        next_element = next_element->next;
    }

    return next_element;
}

struct wl_list *viv_wl_list_prev_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element) {
    // This can only be safely called if the list is guaranteed to have another element to find
    ASSERT(wl_list_length(root_element) > 1);

    struct wl_list *prev_element = cur_element->prev;
    if (prev_element == root_element) {
        prev_element = prev_element->prev;
    }

    return prev_element;
}
