#ifndef VIV_WL_LIST_UTILS_H
#define VIV_WL_LIST_UTILS_H

struct wl_list *viv_wl_list_next_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element);
struct wl_list *viv_wl_list_prev_ignoring_root(struct wl_list *cur_element, struct wl_list *root_element);

#endif
