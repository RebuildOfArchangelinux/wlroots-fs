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
		struct wl_signal new_fractional_scale; // struct *wlr_fractional_scale_v1
	} events;

	// private state

	struct wl_listener display_destroy;
};

struct wlr_fractional_scale_v1 {
	struct wl_resource *resource;

	struct wlr_surface *surface;
	double factor;

	struct {
		struct wl_signal destroy;
		struct wl_signal set_scale_factor;
	} events;

	// private state

	struct wlr_addon addon;
};

struct wlr_fractional_scale_global *wlr_fractional_scale_global_create(
	struct wl_display *display);

struct wlr_fractional_scale_v1 *wlr_fractional_scale_v1_from_surface(
	struct wlr_surface *surface);

void wlr_fractional_scale_v1_send_scale_factor(
	struct wlr_fractional_scale_v1 *scale, double factor);

double wlr_fractional_scale_v1_get_surface_factor(
	struct wlr_surface *surface);

#endif
