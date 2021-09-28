#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "viv_types.h"
#include "viv_view.h"

static void viv_wl_array_append_view(struct wl_array *array, struct viv_view *view) {
    *(struct viv_view **)wl_array_add(array, sizeof(struct viv_view *)) = view;
}

static uint32_t viv_wl_array_num_views(struct wl_array *array) {
    return array->size / sizeof(struct viv_view *);
}

static void distribute_views(struct wl_array *views, struct wl_array *main_box, struct wl_array *secondary_box, uint32_t main_box_count) {
    struct viv_view **view_ptr;
    uint32_t main_count = 0;
    uint32_t secondary_count = 0;
    wl_array_for_each(view_ptr, views) {
        struct viv_view *view = *view_ptr;
        if (main_count < main_box_count) {
            viv_wl_array_append_view(main_box, view);
            main_count++;
        } else {
            viv_wl_array_append_view(secondary_box, view);
            secondary_count++;
        }
    }
}

/// Layout the views in the given rectangle, one above the other, with heights as equal as possible.
static void layout_views_in_column(struct wl_array *views, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    struct viv_view *view;
    uint32_t num_views = viv_wl_array_num_views(views);
    if (!num_views) {
        wlr_log(WLR_ERROR, "Asked to layout views in column, but view count was 0?");
        return;
    }

    uint32_t view_height = (uint32_t)((float)height / (float)num_views);
    uint32_t spare_pixels = height - num_views * view_height;
    uint32_t spare_pixels_used = 0;

    uint32_t cur_y = y;
    struct viv_view **view_ptr;
    wl_array_for_each(view_ptr, views) {
        view = *view_ptr;
        uint32_t target_height = view_height;
        if (spare_pixels) {
            spare_pixels--;
            spare_pixels_used++;
            target_height++;
        }
        wlr_log(WLR_INFO, "Setting target box x %d y %d width %d height %d (num views %d)", x, cur_y, width, target_height, num_views);
        viv_view_set_target_box(view, x, cur_y, width, target_height);

        cur_y += target_height;
    }
}

/// Layout the views in the given rectangle, each next to the others, with widths as equal as possible.
static void layout_views_in_row(struct wl_array *views, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    struct viv_view *view;
    uint32_t num_views = viv_wl_array_num_views(views);
    if (!num_views) {
        wlr_log(WLR_ERROR, "Asked to layout views in column, but view count was 0?");
        return;
    }

    uint32_t view_width = (uint32_t)((float)width / (float)num_views);
    uint32_t spare_pixels = width - num_views * view_width;
    uint32_t spare_pixels_used = 0;

    uint32_t cur_x = x;
    struct viv_view **view_ptr;
    wl_array_for_each(view_ptr, views) {
        view = *view_ptr;
        uint32_t target_width = view_width;
        if (spare_pixels) {
            spare_pixels--;
            spare_pixels_used++;
            target_width++;
        }
        viv_view_set_target_box(view, cur_x, y, target_width, height);

        cur_x += target_width;
    }
}

void viv_layout_do_columns(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    UNUSED(float_param);
    UNUSED(counter_param);
    layout_views_in_row(views, 0, 0, width, height);
}


/**
 *  |--------------|---------|
 *  |              |         |
 *  |              |    2    |
 *  |              |         |
 *  |     MAIN     |----|----|
 *  |              |    | 4  |
 *  |              | 3  |----|
 *  |              |    | 5  |
 *  |--------------|----|----|
 */
void viv_layout_do_fibonacci_spiral(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    UNUSED(counter_param);
    uint32_t num_views = viv_wl_array_num_views(views);

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t available_width = width;
    uint32_t available_height = height;

    uint32_t view_index = 0;
    struct viv_view **view_ptr;
    wl_array_for_each(view_ptr, views) {
        struct viv_view *view = *view_ptr;
        bool is_last_view = (view_index == num_views - 1);
        if (view_index % 2 == 0) {

            uint32_t cur_width = (uint32_t)(float_param * available_width);
            if (is_last_view) {
                cur_width = available_width;
            }

            viv_view_set_target_box(view, x, y, cur_width, available_height);

            x += cur_width;
            available_width -= cur_width;
        } else {
            uint32_t cur_height = (uint32_t)(float_param * available_height);

            if (is_last_view) {
                cur_height = available_height;
            }

            viv_view_set_target_box(view, x, y, available_width, cur_height);

            y += cur_height;
            available_height -= cur_height;
        }

        view_index++;
    }
}

/**
 *  |------|----------|------|
 *  |      |          |  2   |
 *  |  3   |          |------|
 *  |      |          |  4   |
 *  |------|   MAIN   |      |
 *  |      |          |------|
 *  |  5   |          |  6   |
 *  |      |          |      |
 *  |------|----------|------|
 */
void viv_layout_do_central_column(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    uint32_t num_views = viv_wl_array_num_views(views);
    float split_dist = (num_views == 0) ? 1.0 : float_param;

    uint32_t central_column_width = (uint32_t)(width * split_dist);
    if (counter_param == 0) {
        central_column_width = 0;
    } else if (num_views <= counter_param) {
        central_column_width = width;
    }

    uint32_t num_non_main_views = (counter_param > num_views) ? 0 : num_views - counter_param;
    // Do allocation in this order so that if the number of views is
    // odd, the extra one ends up in the right column
    uint32_t num_right_views = num_non_main_views / 2;
    uint32_t num_left_views = num_non_main_views - num_right_views;

    uint32_t remaining_width = width - central_column_width;
    uint32_t left_column_width = remaining_width / 2;
    uint32_t right_column_width = remaining_width - left_column_width;
    if (num_right_views == 0) {
        central_column_width += right_column_width;
        right_column_width = 0;
    }

    struct wl_array main_box, secondary_box;
    wl_array_init(&main_box);
    wl_array_init(&secondary_box);
    distribute_views(views, &main_box, &secondary_box, counter_param);

    layout_views_in_column(&main_box, left_column_width, 0, central_column_width, height);


    struct wl_array left_box, right_box;
    wl_array_init(&left_box);
    wl_array_init(&right_box);

    struct viv_view **view_ptr;
    uint32_t col_counter_param = 0;
    wl_array_for_each(view_ptr, &secondary_box) {
        struct viv_view *view = *view_ptr;
        if (col_counter_param >= num_left_views) {
            viv_wl_array_append_view(&right_box, view);
        } else {
            viv_wl_array_append_view(&left_box, view);
        }
        col_counter_param++;
    }

    layout_views_in_column(&left_box, 0, 0, left_column_width, height);
    layout_views_in_column(&right_box, left_column_width + central_column_width, 0,
                           right_column_width, height);

    wl_array_release(&main_box);
    wl_array_release(&secondary_box);
    wl_array_release(&left_box);
    wl_array_release(&right_box);
}


/**
 *          |--------|
 *          |   2    |
 *      |---|        |---|
 *  |---|   |--------|   |---|
 *  | 5 |      MAIN      | 3 |
 *  |---|                |---|
 *      |----------------|
 *          |   4    |
 *          |--------|
 */
void viv_layout_do_indented_tabs(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    UNUSED(views);
    UNUSED(float_param);
    UNUSED(counter_param);
    UNUSED(width);
    UNUSED(height);
}

/**
 *  |------------------------|
 *  |                        |
 *  |                        |
 *  |                        |
 *  |          MAIN          |
 *  |                        |
 *  |                        |
 *  |                        |
 *  |------------------------|
 */
void viv_layout_do_fullscreen(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    UNUSED(float_param);
    UNUSED(counter_param);
    struct viv_view **view_ptr;
    wl_array_for_each(view_ptr, views) {
        struct viv_view *view = *view_ptr;
        viv_view_set_target_box(view, 0, 0, width, height);
    }
}

/**
 *  |--------------|---------|
 *  |              |    2    |
 *  |              |---------|
 *  |              |    3    |
 *  |     MAIN     |---------|
 *  |              |    4    |
 *  |              |---------|
 *  |              |    5    |
 *  |--------------|---------|
 */
void viv_layout_do_split(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    uint32_t num_views = viv_wl_array_num_views(views);
    float split_dist = (num_views == 0) ? 1.0 : float_param;

    uint32_t split_pixel = (uint32_t)(width * split_dist);
    if (counter_param == 0) {
        split_pixel = 0;
    } else if (num_views <= counter_param) {
        split_pixel = width;
    }

    struct wl_array main_box, secondary_box;
    wl_array_init(&main_box);
    wl_array_init(&secondary_box);
    distribute_views(views, &main_box, &secondary_box, counter_param);

    layout_views_in_column(&main_box, 0, 0, split_pixel, height);
    if (num_views > counter_param) {
        layout_views_in_column(&secondary_box, split_pixel, 0, width - split_pixel, height);
    }

    wl_array_release(&main_box);
    wl_array_release(&secondary_box);
}

void viv_layout_apply(struct viv_workspace *workspace, uint32_t width, uint32_t height) {
    struct wl_array views_array;
    wl_array_init(&views_array);
    struct viv_view *view;
    wl_list_for_each(view, &workspace->views, workspace_link) {
        // Pull out only the non-floating views to be laid out
        if (view->is_floating || (view->workspace->fullscreen_view == view)) {
            continue;
        }
        viv_wl_array_append_view(&views_array, view);
    }

    float float_param = workspace->active_layout->parameter;
    uint32_t counter_param = workspace->active_layout->counter;

    workspace->active_layout->layout_function(&views_array, float_param, counter_param, width, height);
}
