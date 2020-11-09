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

void do_split_layout(struct viv_workspace *workspace) {
    struct wl_list *views = &workspace->views;
    struct viv_output *output = workspace->output;

    int32_t width = output->wlr_output->width;
    int32_t height = output->wlr_output->height;

    struct viv_view *view;

    uint32_t num_views = 0;
    wl_list_for_each(view, views, workspace_link) {
        if (view->mapped) {
            num_views++;
        }
    }

    float split_dist = workspace->divide;
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

    uint32_t view_index = 0;
    wl_list_for_each(view, views, workspace_link) {
        if (!view->mapped) {
            continue;
        }
        printf("doing view_index %d\n", view_index);
        struct wlr_box geo_box;
        wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

        if (view_index == 0) {
            view->x = 0 - geo_box.x;
            view->y = 0 - geo_box.y;
            wlr_xdg_toplevel_set_size(view->xdg_surface, split_pixel, height);
        } else {
            view->x = split_pixel - geo_box.x;
            view->y = ((view_index - 1) * side_bar_view_height) - geo_box.y;
            uint32_t target_height = side_bar_view_height;
            if (spare_pixels) {
                spare_pixels--;
                spare_pixels_used++;
                target_height++;
                (view->y) = view->y - spare_pixels_used;
                printf("Used a spare pixel\n");
            }
            wlr_xdg_toplevel_set_size(view->xdg_surface, width - split_pixel,
                                      target_height);
        }

        printf("This view's geometry became: x %d, y %d, width %d, height %d\n",
               geo_box.x, geo_box.y, geo_box.width, geo_box.height);

        view_index++;
    }
}
