
#include <wayland-util.h>
#include <wlr/types/wlr_surface.h>

#include "viv_damage.h"
#include "viv_output.h"
#include "viv_types.h"
#include "viv_wlr_surface_tree.h"

static struct viv_surface_tree_node *viv_surface_tree_create(struct viv_server *server,  struct wlr_surface *surface);
static struct viv_surface_tree_node *viv_surface_tree_subsurface_node_create(struct viv_server *server, struct viv_surface_tree_node *parent,
                                                                             struct viv_wlr_subsurface *subsurface, struct wlr_surface *surface);
static void viv_subsurface_destroy(struct viv_wlr_subsurface *subsurface);

/// Walks up the surface tree until reaching the root, adding all the surface offsets along the way
static void add_surface_global_offset(struct viv_surface_tree_node *node, int *lx, int *ly) {
    *lx += node->wlr_surface->sx;
    *ly += node->wlr_surface->sy;

    if (node->apply_global_offset) {
        // This is the root node
        ASSERT(!node->subsurface);
        node->apply_global_offset(node->global_offset_data, lx, ly);
    } else {
        ASSERT(node->subsurface);

        *lx += node->subsurface->wlr_subsurface->current.x;
        *ly += node->subsurface->wlr_subsurface->current.y;

        add_surface_global_offset(node->parent, lx, ly);
    }
}

static void handle_subsurface_map (struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, map);
    struct wlr_subsurface *wlr_subsurface = subsurface->wlr_subsurface;


    subsurface->child = viv_surface_tree_subsurface_node_create(subsurface->server, subsurface->parent, subsurface, wlr_subsurface->surface);

    wlr_log(WLR_INFO, "Mapped subsurface at %p creates node at %p", subsurface, subsurface->child);
}

static void handle_subsurface_unmap (struct wl_listener *listener, void *data) {
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, unmap);

    wlr_log(WLR_INFO, "Unmapped subsurface at %p with child %p", subsurface, subsurface->child);

    if (subsurface->child) {

        struct viv_surface_tree_node *node = subsurface->child;

        int lx = 0;
        int ly = 0;
        add_surface_global_offset(node, &lx, &ly);

        struct wlr_box surface_extents = { 0 };
        wlr_surface_get_extends(node->wlr_surface, &surface_extents);
        surface_extents.x += lx;
        surface_extents.y += ly;

        struct viv_output *output;
        wl_list_for_each(output, &node->server->outputs, link) {
            viv_output_damage_layout_coords_box(output, &surface_extents);
        }

        viv_surface_tree_destroy(subsurface->child);
        subsurface->child = NULL;
    }

    UNUSED(data);
}

static void handle_subsurface_destroy (struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, destroy);
    wl_list_remove(&subsurface->node_link);
    wlr_log(WLR_INFO, "Destroyed subsurface at %p", subsurface);
    free(subsurface);
}

static void handle_new_node_subsurface (struct wl_listener *listener, void *data) {
    struct viv_surface_tree_node *node = wl_container_of(listener, node, new_subsurface);

    struct wlr_subsurface *wlr_subsurface = data;

    struct viv_wlr_subsurface *subsurface = calloc(1, sizeof(struct viv_wlr_subsurface));
    CHECK_ALLOCATION(subsurface);

    wlr_log(WLR_INFO, "New subsurface at %p", subsurface);

    subsurface->server = node->server;
    subsurface->wlr_subsurface = wlr_subsurface;
    subsurface->parent = node;

    subsurface->map.notify = handle_subsurface_map;
    wl_signal_add(&wlr_subsurface->events.map, &subsurface->map);

    subsurface->unmap.notify = handle_subsurface_unmap;
    wl_signal_add(&wlr_subsurface->events.unmap, &subsurface->unmap);

    subsurface->destroy.notify = handle_subsurface_destroy;
    wl_signal_add(&wlr_subsurface->events.destroy, &subsurface->destroy);

    struct wlr_subsurface *existing_wlr_subsurface;
    wl_list_for_each(existing_wlr_subsurface, &wlr_subsurface->surface->current.subsurfaces_below, current.link) {
        handle_new_node_subsurface(&node->new_subsurface, existing_wlr_subsurface);
    }
    wl_list_for_each(existing_wlr_subsurface, &wlr_subsurface->surface->current.subsurfaces_above, current.link) {
        handle_new_node_subsurface(&node->new_subsurface, existing_wlr_subsurface);
    }

    wl_list_insert(&node->child_subsurfaces, &subsurface->node_link);
}

static void handle_commit(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_surface_tree_node *node = wl_container_of(listener, node, commit);
    struct wlr_surface *surface = node->wlr_surface;

    int lx = 0;
    int ly = 0;
    add_surface_global_offset(node, &lx, &ly);
    viv_damage_surface(node->server, surface, lx, ly);
}

static void handle_node_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_surface_tree_node *node = wl_container_of(listener, node, destroy);

    wlr_log(WLR_INFO, "Destroy node at %p", node);

    struct viv_wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &node->child_subsurfaces, node_link) {
        viv_subsurface_destroy(subsurface);
    }

    wl_list_remove(&node->new_subsurface.link);
    wl_list_remove(&node->commit.link);
    wl_list_remove(&node->destroy.link);

    free(node);
}


static struct viv_surface_tree_node *viv_surface_tree_create(struct viv_server *server,  struct wlr_surface *surface) {
    struct viv_surface_tree_node *node = calloc(1, sizeof(struct viv_surface_tree_node));
    CHECK_ALLOCATION(node);

    wlr_log(WLR_INFO, "New node at %p", node);

    node->server = server;
    node->wlr_surface = surface;

    wl_list_init(&node->child_subsurfaces);

    node->new_subsurface.notify = handle_new_node_subsurface;
    wl_signal_add(&surface->events.new_subsurface, &node->new_subsurface);

    node->commit.notify = handle_commit;
    wl_signal_add(&surface->events.commit, &node->commit);

    node->destroy.notify = handle_node_destroy;
    wl_signal_add(&surface->events.destroy, &node->destroy);

    struct wlr_subsurface *wlr_subsurface;
    wl_list_for_each(wlr_subsurface, &surface->current.subsurfaces_below, current.link) {
        handle_new_node_subsurface(&node->new_subsurface, wlr_subsurface);
    }
    wl_list_for_each(wlr_subsurface, &surface->current.subsurfaces_above, current.link) {
        handle_new_node_subsurface(&node->new_subsurface, wlr_subsurface);
    }

    return node;
}

static struct viv_surface_tree_node *viv_surface_tree_subsurface_node_create(struct viv_server *server, struct viv_surface_tree_node *parent, struct viv_wlr_subsurface *subsurface, struct wlr_surface *surface) {
    ASSERT(server);
    ASSERT(parent);
    ASSERT(subsurface);
    ASSERT(surface);

    struct viv_surface_tree_node *node = viv_surface_tree_create(server, surface);
    node->parent = parent;
    node->subsurface = subsurface;

    return node;
}

struct viv_surface_tree_node *viv_surface_tree_root_create(struct viv_server *server, struct wlr_surface *surface, void (*apply_global_offset)(void *, int *, int *), void *global_offset_data) {
    ASSERT(server);
    ASSERT(surface);
    ASSERT(apply_global_offset);

    struct viv_surface_tree_node *node = viv_surface_tree_create(server, surface);
    node->apply_global_offset = apply_global_offset;
    node->global_offset_data = global_offset_data;

    return node;
}

/// Clean up the subsurface state (unbind events etc.), including for all children, and
/// free it
static void viv_subsurface_destroy(struct viv_wlr_subsurface *subsurface) {
    if (subsurface->child) {
        viv_surface_tree_destroy(subsurface->child);
        subsurface->child = NULL;
    }

    wl_list_remove(&subsurface->map.link);
    wl_list_remove(&subsurface->unmap.link);
    wl_list_remove(&subsurface->destroy.link);

    free(subsurface);
}

void viv_surface_tree_destroy(struct viv_surface_tree_node *node) {
    struct viv_wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &node->child_subsurfaces, node_link) {
        viv_subsurface_destroy(subsurface);
    }

    wl_list_remove(&node->new_subsurface.link);
    wl_list_remove(&node->commit.link);
    wl_list_remove(&node->destroy.link);

    free(node);
}
