#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
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
#include "viv_workspace.h"

/**
 *  |--------------|----|----|
 *  |              | 4  |    |
 *  |              |----| 3  |
 *  |              | 5  |    |
 *  |     MAIN     |----|----|
 *  |              |         |
 *  |              |    2    |
 *  |              |         |
 *  |--------------|---------|
 */
void viv_layout_do_fibonacci_spiral(struct viv_workspace *workspace, uint32_t width, uint32_t height) {
    struct wl_list *views = &workspace->views;

    struct viv_view *view;

    uint32_t num_views = viv_workspace_num_tiled_views(workspace);

    float frac = workspace->active_layout->parameter;

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t available_width = width;
    uint32_t available_height = height;

    uint32_t view_index = 0;
    wl_list_for_each(view, views, workspace_link) {
        bool is_last_view = (view_index == num_views - 1);
        if (view_index % 2 == 0) {

            uint32_t cur_width = (uint32_t)(frac * available_width);
            if (is_last_view) {
                cur_width = available_width;
            }

            viv_view_set_target_box(view, x, y, cur_width, available_height);

            x += cur_width;
            available_width -= cur_width;
        } else {
            uint32_t cur_height = (uint32_t)(frac * available_height);

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
void viv_layout_do_central_column(struct viv_workspace *workspace) {
    UNUSED(workspace);
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
void viv_layout_do_indented_tabs(struct viv_workspace *workspace) {
    UNUSED(workspace);
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
void viv_layout_do_fullscreen(struct viv_workspace *workspace, uint32_t width, uint32_t height) {
    struct wl_list *views = &workspace->views;

    struct viv_view *view;
    wl_list_for_each(view, views, workspace_link) {
        if (view->is_floating) {
            continue;
        }
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
void viv_layout_do_split(struct viv_workspace *workspace, uint32_t width, uint32_t height) {
    struct wl_list *views = &workspace->views;

    struct viv_view *view;

    uint32_t num_views = viv_workspace_num_tiled_views(workspace);

    float split_dist = workspace->active_layout->parameter;
    if (num_views == 1) {
        split_dist = 1.0;
    }

    uint32_t split_pixel = (uint32_t)(width * split_dist);

    uint32_t side_bar_view_height = (uint32_t)(
                                               (num_views > 1) ? ((float)height / ((float)num_views - 1)) : 100);
    uint32_t spare_pixels = height - (num_views - 1) * side_bar_view_height;
    uint32_t spare_pixels_used = 0;

    uint32_t view_index = 0;
    wl_list_for_each(view, views, workspace_link) {
        if (view->is_floating) {
            continue;
        }

        if (view_index == 0) {
            viv_view_set_target_box(view, 0, 0, split_pixel, height);
        } else {
            uint32_t target_height = side_bar_view_height;
            if (spare_pixels) {
                spare_pixels--;
                spare_pixels_used++;
                target_height++;
                (view->y) = view->y - spare_pixels_used;
            }

            viv_view_set_target_box(view, split_pixel, (view_index - 1) * side_bar_view_height + spare_pixels_used, width - split_pixel, target_height);
        }

        view_index++;
    }

}
