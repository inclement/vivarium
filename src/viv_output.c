#include "viv_output.h"

#include "viv_types.h"

#include <wlr/types/wlr_output_layout.h>

struct viv_output *viv_output_at(struct viv_server *server, double lx, double ly) {

    struct wlr_output *wlr_output_at_point = wlr_output_layout_output_at(server->output_layout, lx, ly);

    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output == wlr_output_at_point) {
            return output;
        }
    }
    return NULL;
}

void viv_output_make_active(struct viv_output *output) {
    if (output == NULL) {
        // It's acceptable to be asked to make a NULL output active
        // TODO: is it really?
        return;
    }

    output->server->active_output = output;
}
