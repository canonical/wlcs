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
    char const* name;
    uint32_t version;
};

typedef struct WlcsIntegrationDescriptor WlcsIntegrationDescriptor;
struct WlcsIntegrationDescriptor
{
    uint32_t version;

    size_t num_extensions;
    WlcsExtensionDescriptor const* supported_extensions;
};

#define WLCS_DISPLAY_SERVER_VERSION 2
typedef struct WlcsDisplayServer WlcsDisplayServer;
struct WlcsDisplayServer
{
    uint32_t version;

    void (*start)(WlcsDisplayServer* server);
    void (*stop)(WlcsDisplayServer* server);

    int (*create_client_socket)(WlcsDisplayServer* server);

    void (*position_window_absolute)(WlcsDisplayServer* server, wl_display* client, wl_surface* surface, int x, int y);

    WlcsPointer* (*create_pointer)(WlcsDisplayServer* server);
    WlcsTouch* (*create_touch)(WlcsDisplayServer* server);

    /* Added in version 2 */
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
    uint32_t version;

    WlcsDisplayServer* (*create_server)(int argc, char const** argv);
    void (*destroy_server)(WlcsDisplayServer* server);
};

extern WlcsServerIntegration const wlcs_server_integration;

#ifdef __cplusplus
}
#endif

#endif //WLCS_SERVER_H_
