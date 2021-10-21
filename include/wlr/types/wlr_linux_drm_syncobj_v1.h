/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_LINUX_DRM_SYNCOBJ_V1_H
#define WLR_TYPES_WLR_LINUX_DRM_SYNCOBJ_V1_H

#include <wayland-server-core.h>
#include <wlr/util/addon.h>

struct wlr_linux_drm_syncobj_surface_v1_state {
	struct wlr_render_timeline *acquire_timeline;
	uint64_t acquire_point;

	struct wlr_render_timeline *release_timeline;
	uint64_t release_point;
};

struct wlr_linux_drm_syncobj_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_linux_drm_syncobj_surface_v1_state pending, current;

	// private state

	struct wlr_addon addon;

	struct wl_listener surface_client_commit;
};

struct wlr_linux_drm_syncobj_v1 {
	struct wl_global *global;
	int drm_fd;

	struct {
		struct wl_signal destroy;
	} events;

	// private state

	struct wl_listener display_destroy;
};

/**
 * Advertise explicit synchronization support to clients.
 *
 * The compositor must be prepared to handle fences coming from clients and to
 * send release fences correctly. In particular, both the renderer and the
 * backend need to support explicit synchronization.
 */
struct wlr_linux_drm_syncobj_v1 *wlr_linux_drm_syncobj_v1_create(
	struct wl_display *display, int drm_fd);

struct wlr_linux_drm_syncobj_surface_v1_state *
wlr_linux_drm_syncobj_v1_get_surface_state(
	struct wlr_linux_drm_syncobj_v1 *explicit_sync,
	struct wlr_surface *surface);

#endif
