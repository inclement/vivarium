
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

void viv_wl_list_swap(struct wl_list *elm1, struct wl_list *elm2) {
    struct wl_list *elm1_prev = elm1->prev;
    struct wl_list *elm1_next = elm1->next;

    // TODO: This can be done more simply

    elm1->prev = elm2->prev;
    elm1->next = elm2->next;

    elm2->prev = elm1_prev;
    elm2->next = elm1_next;

    // If the two list items were next to one another then they will have gained circular
    // references, so fix these before correcting references of adjacent elements.
    if (elm1->next == elm1) {
        elm1->next = elm2;
    }
    if (elm1->prev == elm1) {
        elm1->prev = elm2;
    }
    if (elm2->next == elm2) {
        elm2->next = elm1;
    }
    if (elm2->next == elm2) {
        elm2->next = elm1;
    }

    elm1->prev->next = elm1;
    elm1->next->prev = elm1;

    elm2->prev->next = elm2;
    elm2->next->prev = elm2;
}
