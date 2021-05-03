
#include "viv_damage.h"
#include "viv_output.h"
#include "viv_types.h"

static void handle_popup_surface_commit(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, surface_commit);
    struct wlr_surface *surface = popup->wlr_popup->base->surface;

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    wlr_surface_get_effective_damage(surface, &damage);

    int px = *popup->lx + popup->wlr_popup->geometry.x;
    int py = *popup->ly + popup->wlr_popup->geometry.y;
    pixman_region32_translate(&damage, px, py);

    viv_damage_surface(popup->server, surface, px, py);

    pixman_region32_fini(&damage);
}

static void handle_popup_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, surface_unmap);

    int px = *popup->lx + popup->wlr_popup->geometry.x;
    int py = *popup->ly + popup->wlr_popup->geometry.y;
    struct wlr_box geo_box = {
        .x = px,
        .y = py,
        .width = popup->wlr_popup->geometry.width,
        .height = popup->wlr_popup->geometry.height,
    };

    struct viv_output *output;
    wl_list_for_each(output, &popup->server->outputs, link) {
        viv_output_damage_layout_coords_box(output, &geo_box);
    }
}

static void handle_popup_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, destroy);
    wlr_log(WLR_INFO, "Popup being destroyed");
    free(popup);
}

void viv_xdg_popup_init(struct viv_xdg_popup *popup, struct wlr_xdg_popup *wlr_popup) {
    popup->wlr_popup = wlr_popup;

    popup->surface_commit.notify = handle_popup_surface_commit;
    wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->surface_commit);

    popup->surface_unmap.notify = handle_popup_surface_unmap;
    wl_signal_add(&wlr_popup->base->events.unmap, &popup->surface_unmap);

    popup->destroy.notify = handle_popup_surface_destroy;
    wl_signal_add(&wlr_popup->base->surface->events.destroy, &popup->destroy);
}
