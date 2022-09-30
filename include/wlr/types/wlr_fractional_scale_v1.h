/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_XDG_ACTIVATION_V1
#define WLR_TYPES_WLR_XDG_ACTIVATION_V1

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/util/addon.h>

struct wlr_fractional_scale_global {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	// private state

	struct wl_listener display_destroy;
};

struct wlr_fractional_scale_global *wlr_fractional_scale_global_create(
	struct wl_display *display);

bool wlr_fractional_scale_v1_send_scale_factor(
	struct wlr_surface *surface, double factor);

#endif
