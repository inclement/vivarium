
#include <pixman-1/pixman.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_damage.h>

#include "viv_output.h"
#include "viv_types.h"


/// Apply the damage from this surface to every output
void viv_damage_surface(struct viv_server *server, struct wlr_surface *surface, int lx, int ly) {
    pixman_region32_t damage;
    pixman_region32_init(&damage);
    wlr_surface_get_effective_damage(surface, &damage);

    pixman_region32_translate(&damage, lx, ly);

    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        viv_output_damage_layout_coords_region(output, &damage);
    }

    pixman_region32_fini(&damage);
}
