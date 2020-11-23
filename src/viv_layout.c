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
void viv_layout_do_fibonacci_spiral(struct viv_workspace *workspace) {
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
void viv_layout_do_fullscreen(struct viv_workspace *workspace) {
    struct wl_list *views = &workspace->views;
    struct viv_output *output = workspace->output;

    int32_t width = output->wlr_output->width;
    int32_t height = output->wlr_output->height;

    struct viv_view *view;
    wl_list_for_each(view, views, workspace_link) {
        if (!view->mapped) {
            continue;
        }
        if (view->is_floating) {
            continue;
        }

        struct wlr_box geo_box;
        wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

        view->x = 0 - geo_box.x;
        view->y = 0 - geo_box.y;
        wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);

        view->target_x = 0;
        view->target_y = 0;
        view->target_width = (int)width;
        view->target_height = (int)height;

        printf("This view's geometry became: x %d, y %d, width %d, height %d\n",
                geo_box.x, geo_box.y, geo_box.width, geo_box.height);
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
void viv_layout_do_split(struct viv_workspace *workspace) {
    struct wl_list *views = &workspace->views;
    struct viv_output *output = workspace->output;

    int32_t width = output->wlr_output->width;
    int32_t height = output->wlr_output->height;

    struct viv_view *view;

    uint32_t num_views = 0;
    wl_list_for_each(view, views, workspace_link) {
        if (view->mapped) {
            if (!view->is_floating) {
                num_views++;
            }
        }
    }

    float split_dist = workspace->active_layout->parameter;
    if (num_views == 1) {
        split_dist = 1.0;
    }

    uint32_t split_pixel = (uint32_t)(width * split_dist);

    uint32_t side_bar_view_height = (uint32_t)(
                                               (num_views > 1) ? ((float)height / ((float)num_views - 1)) : 100);
    uint32_t spare_pixels = height - (num_views - 1) * side_bar_view_height;
    uint32_t spare_pixels_used = 0;
    printf("Have to use %d spare pixels\n", spare_pixels);
    printf("Laying out %d views\n", num_views);

    int border_width = output->server->config->border_width;

    uint32_t view_index = 0;
    wl_list_for_each(view, views, workspace_link) {
        if (!view->mapped) {
            continue;
        }
        printf("doing view_index %d\n", view_index);
        struct wlr_box geo_box;
        wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

        if (!view->is_floating) {
            if (view_index == 0) {
                view->x = 0 - geo_box.x + border_width;
                view->y = 0 - geo_box.y + border_width;
                wlr_xdg_toplevel_set_size(view->xdg_surface,
                                          split_pixel - 2 * border_width,
                                          height - 2 * border_width);

                view->target_x = 0;
                view->target_y = 0;
                view->target_width = (int)split_pixel;
                view->target_height = (int)height;
            } else {
                view->x = split_pixel - geo_box.x + border_width;
                view->y = ((view_index - 1) * side_bar_view_height) - geo_box.y + border_width;
                uint32_t target_height = side_bar_view_height;
                if (spare_pixels) {
                    spare_pixels--;
                    spare_pixels_used++;
                    target_height++;
                    (view->y) = view->y - spare_pixels_used;
                    printf("Used a spare pixel\n");
                }
                wlr_xdg_toplevel_set_size(view->xdg_surface,
                                          width - split_pixel - 2 * border_width,
                                          target_height - 2 * border_width);

                view->target_x = split_pixel;
                view->target_y = (view_index - 1) * side_bar_view_height - spare_pixels_used;
                view->target_width = (int)(width - split_pixel);
                view->target_height = (int)target_height;
            }

            printf("This view's geometry became: x %d, y %d, width %d, height %d\n",
                geo_box.x, geo_box.y, geo_box.width, geo_box.height);

            view_index++;
        }
    }
}
