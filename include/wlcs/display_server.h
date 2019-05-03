/*
 * Copyright © 2017 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */
#ifndef WLCS_SERVER_H_
#define WLCS_SERVER_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wl_surface wl_surface;
typedef struct wl_display wl_display;
typedef struct wl_event_loop wl_event_loop;

typedef struct WlcsPointer WlcsPointer;
typedef struct WlcsTouch WlcsTouch;

#define WLCS_INTEGRATION_DESCRIPTOR_VERSION 1
typedef struct WlcsExtensionDescriptor WlcsExtensionDescriptor;
struct WlcsExtensionDescriptor
{
    /**
     * Protocol name of extension (eg: wl_shell, xdg_shell, etc)
     */
    char const* name;
    /**
     * Maximum version of extension supported
     */
    uint32_t version;
};

typedef struct WlcsIntegrationDescriptor WlcsIntegrationDescriptor;
struct WlcsIntegrationDescriptor
{
    uint32_t version; /**< Version of the struct this instance provides */

    size_t num_extensions; /**< Length of the supported_extensions array */
    /**
     * Array of extension descriptions
     *
     * This must be an array of length num_extensions; that is, accesses from
     * supported_extensions[0] to supported_extensions[num_extensions - 1]
     * must be valid.
     */
    WlcsExtensionDescriptor const* supported_extensions;
};

#define WLCS_DISPLAY_SERVER_VERSION 3
typedef struct WlcsDisplayServer WlcsDisplayServer;
struct WlcsDisplayServer
{
    uint32_t version; /**< Version of the struct this instance provides */

    /**
     * Start the display server's mainloop.
     *
     * This should *not* block until the mainloop exits, which implies the mainloop
     * will need to be run in a separate thread.
     *
     * This does not need to block until the display server is ready to process
     * input, but the WlcsDisplayServer does need to be able to process other
     * calls (notably create_client_socket) once this returns.
     */
    void (*start)(WlcsDisplayServer* server);

    /**
     * Stop the display server's mainloop.
     *
     * In contrast to the start hook, this *should* block until the server's mainloop
     * has been torn down, so that it does not persist into later tests.
     */
    void (*stop)(WlcsDisplayServer* server);

    /**
     * Create a socket that can be connected to by wl_display_connect_fd
     *
     * \return  A FD to a client Wayland socket. WLCS owns this fd, and will close
     *          it as necessary.
     */
    int (*create_client_socket)(WlcsDisplayServer* server);

    /**
     * Position a window in the compositor coördinate space
     *
     * \param client   the (wayland-client-side) wl_client which owns the
     *                  surface
     * \param surface  the (wayland-client-side) wl_surface*
     * \param x         x coördinate (in compositor-space pixels) to move the
     *                  left of the window to
     * \param y         y coördinate (in compositor-space pixels) to move the
     *                  top of the window to
     */
    void (*position_window_absolute)(WlcsDisplayServer* server, wl_display* client, wl_surface* surface, int x, int y);

    /**
     * Create a fake pointer device
     */
    WlcsPointer* (*create_pointer)(WlcsDisplayServer* server);

    /**
     * Create a mock touch object
     */
    WlcsTouch* (*create_touch)(WlcsDisplayServer* server);

    /* Added in version 2 */
    /**
     * Describe the capabilities of this WlcsDisplayServer
     *
     * WLCS will use this description to skip tests that the display server
     * is known to not support. For example, if the set of extensions
     * described by the WlcsIntegrationDescriptor does not include "xdg_shell"
     * then all XDG Shell tests will be skipped.
     *
     * Different WlcsDisplayServer instances may report different
     * capabilities (for example, if command line options should influence
     * the set of extensions exposed).
     */
    WlcsIntegrationDescriptor const* (*get_descriptor)(WlcsDisplayServer const* server);

    /* Added in version 3 */
    /**
     * Start the display server's event loop, blocking the calling thread.
     *
     * When started in this way WLCS will proxy all requests to this mainloop.
     * All calls to WLCS interfaces will be dispatched from the
     * wlcs_event_dispatcher loop, so implementations are required to drive
     * this loop from their own.
     *
     * \note    This is an optional interface. An implementation must provide at
     *          least one of {start, start_on_this_thread}, but does not need to
     *          provide both. If both are provided, start is preferred.

     * \param wlcs_event_dispatcher
     */
    void (*start_on_this_thread)(WlcsDisplayServer* server, wl_event_loop* wlcs_event_dispatcher);
};

#define WLCS_SERVER_INTEGRATION_VERSION 1
typedef struct WlcsServerIntegration WlcsServerIntegration;
struct WlcsServerIntegration
{
    uint32_t version; /**< Version of the struct this instance provides */

    /**
     * Create a WlcsDisplayServer instance
     *
     * This can do any setup necessary, but should not start the compositor's
     * mainloop.
     *
     * \param argc  Command line argument count (after wlcs-specific options
     *              have been stripped)
     * \param argv  Command line arguments (after wlcs-specific options have
     *              been stripped)
     * \return
     */
    WlcsDisplayServer* (*create_server)(int argc, char const** argv);
    void (*destroy_server)(WlcsDisplayServer* server);
};

/**
 * Main WLCS entry point
 *
 * WLCS will load this symbol to discover the compositor integration
 * hooks.
 */
extern WlcsServerIntegration const wlcs_server_integration;

#ifdef __cplusplus
}
#endif

#endif //WLCS_SERVER_H_
