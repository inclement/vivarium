#ifndef VIV_WL_LIST_UTILS_H
#define VIV_WL_LIST_UTILS_H

#include <wayland-util.h>

struct wl_list *viv_wl_list_next_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element);
struct wl_list *viv_wl_list_prev_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element);

/// Switch the given list elements with one another
void viv_wl_list_swap(struct wl_list *elm1, struct wl_list *elm2);

#endif
