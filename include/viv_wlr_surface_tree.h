#ifndef VIV_WLR_SURFACE_TREE_H
#define VIV_WLR_SURFACE_TREE_H

#include "viv_types.h"

struct viv_surface_tree_node {
    struct viv_server *server;
    struct wlr_surface *wlr_surface;

    struct wl_listener new_subsurface;
    struct wl_listener commit;
    struct wl_listener destroy;

    struct viv_surface_tree_node *parent;
    struct viv_wlr_subsurface *subsurface;

    struct wl_list child_subsurfaces;

    void (*apply_global_offset)(void *, int *, int *);
    void *global_offset_data;
};

struct viv_wlr_subsurface {
    struct viv_server *server;
    struct wlr_subsurface *wlr_subsurface;
    struct viv_surface_tree_node *parent;
    struct viv_surface_tree_node *child;

    struct wl_list node_link;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
};


// Create a surface tree from the input surface. The surface tree will automatically wrap
// all of the subsurfaces (existing or later-created) and handle all surface commit
// events.  Commit events will be used to damage every output, with offsets calculated
// including the global offset passed here.
struct viv_surface_tree_node *viv_surface_tree_root_create(struct viv_server *server, struct wlr_surface *surface, void (*apply_global_offset)(void *, int *, int *), void *global_offset_data);

/// Clean up the node's state (bound events etc.) and free it
void viv_surface_tree_destroy(struct viv_surface_tree_node *node);

#endif
