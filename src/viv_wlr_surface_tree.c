
#include <wayland-util.h>
#include <wlr/types/wlr_surface.h>

#include "viv_damage.h"
#include "viv_types.h"
#include "viv_wlr_surface_tree.h"

static struct viv_surface_tree_node *viv_surface_tree_create(struct viv_server *server,  struct wlr_surface *surface);
static struct viv_surface_tree_node *viv_surface_tree_subsurface_node_create(struct viv_server *server, struct viv_surface_tree_node *parent,
                                                                             struct viv_wlr_subsurface *subsurface, struct wlr_surface *surface);

static void handle_subsurface_map (struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, map);
    struct wlr_subsurface *wlr_subsurface = subsurface->wlr_subsurface;

    viv_surface_tree_subsurface_node_create(subsurface->server, subsurface->parent, subsurface, wlr_subsurface->surface);

    wlr_log(WLR_INFO, "Mapped subsurface wlr_surface at %p", wlr_subsurface->surface);
}

static void handle_subsurface_unmap (struct wl_listener *listener, void *data) {
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, unmap);
    // do nothing?
    UNUSED(subsurface);
    UNUSED(data);
    struct wlr_subsurface *wlr_subsurface = subsurface->wlr_subsurface;
    wlr_log(WLR_INFO, "Unmapped subsurface wlr_surface at %p", wlr_subsurface->surface);
}

static void handle_subsurface_destroy (struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_wlr_subsurface *subsurface = wl_container_of(listener, subsurface, destroy);
    wlr_log(WLR_INFO, "Subsurface destroy for wlr_subsurface at %p", subsurface->wlr_subsurface);
    /* free(subsurface);  // ...is this safe? */
}

static void handle_new_node_subsurface (struct wl_listener *listener, void *data) {
    struct viv_surface_tree_node *node = wl_container_of(listener, node, new_subsurface);

    struct wlr_subsurface *wlr_subsurface = data;

    struct viv_wlr_subsurface *subsurface = calloc(1, sizeof(struct viv_wlr_subsurface));
    CHECK_ALLOCATION(subsurface);

    subsurface->server = node->server;
    subsurface->wlr_subsurface = wlr_subsurface;
    subsurface->parent = node;

    subsurface->map.notify = handle_subsurface_map;
    wl_signal_add(&wlr_subsurface->events.map, &subsurface->map);

    subsurface->unmap.notify = handle_subsurface_unmap;
    wl_signal_add(&wlr_subsurface->events.unmap, &subsurface->unmap);

    subsurface->destroy.notify = handle_subsurface_destroy;
    wl_signal_add(&wlr_subsurface->events.destroy, &subsurface->destroy);

    wlr_log(WLR_INFO, "New subsurface for wlr_surface at %p", node->wlr_surface);
}

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
        lx += node->subsurface->wlr_subsurface->current.x;
        ly += node->subsurface->wlr_subsurface->current.y;
        add_surface_global_offset(node->parent, lx, ly);
    }
}

static void handle_commit(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_surface_tree_node *node = wl_container_of(listener, node, commit);
    struct wlr_surface *surface = node->wlr_surface;

    int lx = 0;
    int ly = 0;
    add_surface_global_offset(node, &lx, &ly);
    viv_damage_surface(node->server, surface, lx, ly);

    wlr_log(WLR_INFO, "Committing surface at %p, x %d y %d", surface, lx, ly);
}

static void handle_node_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_surface_tree_node *node = wl_container_of(listener, node, commit);

    wlr_log(WLR_INFO, "Node destroy for node with wlr_surface %p", node->wlr_surface);

    // TODO: Should we destroy all our children? Or are they guaranteed to be destroyed first?
    /* free(node); */
}


static struct viv_surface_tree_node *viv_surface_tree_create(struct viv_server *server,  struct wlr_surface *surface) {
    struct viv_surface_tree_node *node = calloc(1, sizeof(struct viv_surface_tree_node));
    CHECK_ALLOCATION(node);

    node->server = server;
    node->wlr_surface = surface;

    node->new_subsurface.notify = handle_new_node_subsurface;
    wl_signal_add(&surface->events.new_subsurface, &node->new_subsurface);

    node->commit.notify = handle_commit;
    wl_signal_add(&surface->events.commit, &node->commit);

    node->destroy.notify = handle_node_destroy;
    wl_signal_add(&surface->events.destroy, &node->destroy);

    struct wlr_subsurface *wlr_subsurface;
    wl_list_for_each(wlr_subsurface, &surface->subsurfaces, parent_link) {
        handle_new_node_subsurface(&node->new_subsurface, wlr_subsurface);
    }

    wlr_log(WLR_INFO, "Created new viv_surface_tree_node for surface at %p", surface);

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
