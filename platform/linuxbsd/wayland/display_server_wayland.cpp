#include "display_server_wayland.h"

// coherent naming of listeners (to preserve our sanity):
// if structure is A_B_C_listener, then instance is A_B_C_listener_info,
// and callbacks are called on_A_B_C_something().

static const struct wl_registry_listener wl_registry_listener_info = {
    .global = DisplayServerWayland::_on_registry_global,
    .global_remove = DisplayServerWayland::_on_registry_global_remove,
};

static const struct xdg_surface_listener xdg_surface_listener_info = {
    .configure = DisplayServerWayland::_on_xdg_surface_configure,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener_info = {
    .ping = DisplayServerWayland::_on_xdg_wm_base_ping,
};

static const struct wl_buffer_listener wl_buffer_listener_info = {
    .release = DisplayServerWayland::_on_buffer_release,
};

static const xdg_toplevel_listener xdg_toplevel_listener_info = {
    .configure = DisplayServerWayland::_on_xdg_toplevel_configure,
    .close = DisplayServerWayland::_on_xdg_toplevel_close,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter = DisplayServerWayland::_on_pointer_enter,
    .leave = DisplayServerWayland::_on_pointer_leave,
    .motion = DisplayServerWayland::_on_pointer_motion,
    .button = DisplayServerWayland::_on_pointer_button,
    .frame = DisplayServerWayland::_on_pointer_frame,
    //.axis = noop, // TODO
};

static const struct wl_seat_listener wl_seat_listener_info = {
    .capabilities = DisplayServerWayland::_on_seat_handle_capabilities,
    .name = DisplayServerWayland::_on_seat_name,
};

Error DisplayServerWayland::_connect() {
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        ERR_PRINT("Wayland display is not available");
        return ERR_UNAVAILABLE;
    }

    // errors in the following part are unlikely but must be checked for anyway
    // as the alternative is segfaulting on null pointer dereference

    // if something like this happens, we just disconnect and don't bother
    // cleaning up all resources, this should not be a problem in such case

    // the registry holds IDs of the most important objects
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        ERR_PRINT("wayland: wl_display_get_registry() failed, compositor bug?");
        wl_display_disconnect(m_display);
        return ERR_UNAVAILABLE;
    }

    wl_registry_add_listener(m_registry, &wl_registry_listener_info, nullptr);

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base, and seat;
    // for each global, the on_registry_global() callback is called automatically
    wl_display_roundtrip(m_display);

    // check if we have all the needed globals
    if (!m_compositor || !m_shm || !m_xdg_wm_base || !m_seat) {
        ERR_PRINT("wayland: missing one of compositor/shm/xdg_wm_base/seat interfaces, compositor bug?");
        wl_display_disconnect(m_display);
        return ERR_UNAVAILABLE;
    }

    wl_seat_add_listener(m_seat, &wl_seat_listener_info, nullptr);
    xdg_wm_base_add_listener(m_xdg_wm_base, &xdg_wm_base_listener_info, nullptr);

    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        ERR_PRINT("wayland: wl_compositor_create_surface() failed, compositor bug?");
        wl_display_disconnect(m_display);
        return ERR_UNAVAILABLE;
    }

    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    if (!m_xdg_surface) {
        ERR_PRINT("wayland: xdg_wm_base_get_xdg_surface() failed, compositor bug?");
        wl_display_disconnect(m_display);
        return ERR_UNAVAILABLE;
    }

    xdg_surface_add_listener(m_xdg_surface, &xdg_surface_listener_info, nullptr);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    if (!m_xdg_toplevel) {
        ERR_PRINT("wayland: xdg_surface_get_toplevel() failed, compositor bug?");
        wl_display_disconnect(m_display);
    }

    xdg_toplevel_set_title(m_xdg_toplevel, "Godot");
    xdg_toplevel_add_listener(m_xdg_toplevel, &xdg_toplevel_listener_info, nullptr);

    return OK;
}

bool DisplayServerWayland::has_feature(Feature p_feature) const {
    switch (p_feature) {
        case DisplayServer::FEATURE_MOUSE:
        case DisplayServer::FEATURE_HIDPI:
            return true;

		case DisplayServer::FEATURE_GLOBAL_MENU:
		case DisplayServer::FEATURE_SUBWINDOWS:
		case DisplayServer::FEATURE_TOUCHSCREEN:
		case DisplayServer::FEATURE_MOUSE_WARP:
		case DisplayServer::FEATURE_CLIPBOARD:
		case DisplayServer::FEATURE_VIRTUAL_KEYBOARD:
		case DisplayServer::FEATURE_CURSOR_SHAPE:
		case DisplayServer::FEATURE_CUSTOM_CURSOR_SHAPE:
		case DisplayServer::FEATURE_NATIVE_DIALOG:
		case DisplayServer::FEATURE_IME:
		case DisplayServer::FEATURE_WINDOW_TRANSPARENCY:
		case DisplayServer::FEATURE_ICON:
		case DisplayServer::FEATURE_NATIVE_ICON:
		case DisplayServer::FEATURE_ORIENTATION:
		case DisplayServer::FEATURE_SWAP_BUFFERS:
		case DisplayServer::FEATURE_KEEP_SCREEN_ON:
		case DisplayServer::FEATURE_CLIPBOARD_PRIMARY:
		case DisplayServer::FEATURE_TEXT_TO_SPEECH:
		case DisplayServer::FEATURE_EXTEND_TO_TITLE:
		case DisplayServer::FEATURE_SCREEN_CAPTURE:
			return false;
	}
}

String DisplayServerWayland::get_name() const {
    return "wayland";
}

int64_t DisplayServerWayland::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
    if (p_handle_type == HandleType::DISPLAY_HANDLE) {
        return reinterpret_cast<int64_t>(m_display);
    }
    else if (p_handle_type == HandleType::WINDOW_HANDLE) {
        if (p_window == MAIN_WINDOW_ID) {
            return reinterpret_cast<int64_t>(m_surface);
        }
        return 0;
    }
    return 0;
}
