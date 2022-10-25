#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_wrath_v1.h>
#include <wlr/util/log.h>

#include "wp-fractional-scale-wrath-v1-protocol.h"

#define FRACTIONAL_SCALE_WRATH_VERSION 1

struct wlr_fractional_scale_wrath_v1 {
	struct wl_resource *resource;
	struct wlr_addon addon;

	struct wl_listener output_destroy;
};

static const struct wp_fractional_scale_wrath_v1_interface fractional_scale_wrath_interface;

static struct wlr_fractional_scale_wrath_v1 *fractional_scale_wrath_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_fractional_scale_wrath_v1_interface, &fractional_scale_wrath_interface));
	return wl_resource_get_user_data(resource);
}

static const struct wp_fractional_scale_wrath_manager_v1_interface scale_manager_wrath_interface;

// static struct wlr_fractional_scale_wrath_manager_v1 *scale_manager_wrath_v1_from_resource(
// 		struct wl_resource *resource) {
// 	assert(wl_resource_instance_of(resource,
// 		&wp_fractional_scale_wrath_manager_v1_interface,
// 		&scale_manager_wrath_interface));
// 	return wl_resource_get_user_data(resource);
// }

static void fractional_scale_wrath_destroy(struct wlr_fractional_scale_wrath_v1 *info) {
	if (info == NULL) {
		return;
	}

	// If this is a dummy object then we do not have a wl_resource
	if (info->resource != NULL) {
		wl_resource_set_user_data(info->resource, NULL);
	}
	wlr_addon_finish(&info->addon);
	wl_list_remove(&info->output_destroy.link);
	free(info);
}

static void fractional_scale_wrath_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_fractional_scale_wrath_v1 *info =
		fractional_scale_wrath_from_resource(resource);
	fractional_scale_wrath_destroy(info);
}

static void fractional_scale_wrath_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void fractional_scale_wrath_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_fractional_scale_wrath_v1 *info =
		wl_container_of(listener, info, output_destroy);
	fractional_scale_wrath_destroy(info);
}

static const struct wp_fractional_scale_wrath_v1_interface fractional_scale_wrath_interface = {
	.destroy = fractional_scale_wrath_handle_destroy,
};

static void fractional_scale_addon_destroy(struct wlr_addon *addon) {
	struct wlr_fractional_scale_wrath_v1 *info =
		wl_container_of(addon, info, addon);
	fractional_scale_wrath_destroy(info);
}

static const struct wlr_addon_interface fractional_scale_wrath_addon_impl = {
	.name = "wlr_fractional_scale_v1",
	.destroy = fractional_scale_addon_destroy,
};

void wlr_fractional_scale_wrath_v1_notify_scale(struct wlr_output *output) {
	struct wlr_addon *addon = wlr_addon_find(&output->addons,
		&fractional_scale_wrath_addon_impl, &fractional_scale_wrath_addon_impl);

	if (addon == NULL) {
		// Create a dummy object to store the scale
		struct wlr_fractional_scale_wrath_v1 *info = calloc(1, sizeof(*info));
		if (info == NULL) {
			return;
		}

		info->output_destroy.notify =
			fractional_scale_wrath_handle_output_destroy;
		wl_signal_add(&output->events.destroy,
			&info->output_destroy);
		wlr_addon_init(&info->addon, &output->addons,
			&fractional_scale_wrath_addon_impl, &fractional_scale_wrath_addon_impl);
		return;
	}

	struct wlr_fractional_scale_wrath_v1 *info =
		wl_container_of(addon, info, addon);
	if (!info->resource) {
		// Update existing dummy object
		return;
	}

	wp_fractional_scale_wrath_v1_send_scale_fractional(info->resource,
		wl_fixed_from_double(output->scale));
}

static void handle_get_fractional_scale_wrath(struct wl_client *client,
		struct wl_resource *mgr_resource, uint32_t id,
		struct wl_resource *output_resource) {
	struct wlr_output *output =
		wlr_output_from_resource(output_resource);
	// struct wlr_fractional_scale_wrath_manager_v1 *mgr =
	// 	scale_manager_wrath_v1_from_resource(mgr_resource);

	struct wlr_fractional_scale_wrath_v1 *info = NULL;

	// If no fractional_scale object had been created but scale had been set, then
	// there will be a dummy object for us to fill out with a resource. Check if
	// that's the case.
	struct wlr_addon *addon = wlr_addon_find(&output->addons,
		&fractional_scale_wrath_addon_impl, &fractional_scale_wrath_addon_impl);

	if (addon != NULL) {
		info = wl_container_of(addon, info, addon);
		if (info->resource != NULL) {
			wl_resource_post_error(mgr_resource,
				WP_FRACTIONAL_SCALE_WRATH_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
				"a surface scale object for that surface already exists");
			return;
		}
	}

	if (info == NULL) {
		info = calloc(1, sizeof(*info));
		if (info == NULL) {
			wl_client_post_no_memory(client);
			return;
		}

		info->output_destroy.notify =
			fractional_scale_wrath_handle_output_destroy;
		wl_signal_add(&output->events.destroy,
			&info->output_destroy);
		wlr_addon_init(&info->addon, &output->addons,
			&fractional_scale_wrath_addon_impl, &fractional_scale_wrath_addon_impl);
	}

	uint32_t version = wl_resource_get_version(mgr_resource);
	info->resource = wl_resource_create(client,
		&wp_fractional_scale_wrath_v1_interface, version, id);
	if (info->resource == NULL) {
		wl_client_post_no_memory(client);
		fractional_scale_wrath_destroy(info);
		return;
	}
	wl_resource_set_implementation(info->resource,
		&fractional_scale_wrath_interface, info,
		fractional_scale_wrath_handle_resource_destroy);

	// double initial_scale = info->scale != 0.0
	// 	? info->scale
	// 	: mgr->default_scale;
	// wp_fractional_scale_wrath_v1_send_scale_fractional(info->resource,
	// 	wl_fixed_from_double(initial_scale));
}

static void fractional_scale_wrath_manager_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_fractional_scale_wrath_manager_v1_interface scale_manager_wrath_interface = {
	.destroy = fractional_scale_wrath_manager_handle_destroy,
	.get_fractional_scale = handle_get_fractional_scale_wrath,
};

static void fractional_scale_wrath_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_fractional_scale_manager_v1 *mgr = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_fractional_scale_wrath_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &scale_manager_wrath_interface, mgr,
		NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_fractional_scale_wrath_manager_v1 *mgr =
		wl_container_of(listener, mgr, display_destroy);
	wl_signal_emit_mutable(&mgr->events.destroy, NULL);
	wl_global_destroy(mgr->global);
	free(mgr);
}

struct wlr_fractional_scale_wrath_manager_v1 *wlr_fractional_scale_wrath_manager_v1_create(struct wl_display *display) {
	struct wlr_fractional_scale_wrath_manager_v1 *mgr = calloc(1, sizeof(*mgr));
	if (mgr == NULL) {
		return NULL;
	}

	mgr->global = wl_global_create(display,
		&wp_fractional_scale_wrath_manager_v1_interface,
		FRACTIONAL_SCALE_WRATH_VERSION, mgr, fractional_scale_wrath_manager_bind);
	if (mgr->global == NULL) {
		free(mgr);
		return NULL;
	}

	wl_signal_init(&mgr->events.destroy);

	mgr->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &mgr->display_destroy);

	return mgr;
}
