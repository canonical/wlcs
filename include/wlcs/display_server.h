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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wl_surface wl_surface;
typedef struct wl_display wl_display;

typedef struct WlcsPointer WlcsPointer;
typedef struct WlcsTouch WlcsTouch;

#define WLCS_DISPLAY_SERVER_VERSION 1

struct WlcsDisplayServer
{
    uint32_t version;

    void (*start)(WlcsDisplayServer* server);
    void (*stop)(WlcsDisplayServer* server);

    int (*create_client_socket)(WlcsDisplayServer* server);

    void (*position_window_absolute)(WlcsDisplayServer* server, wl_display* client, wl_surface* surface, int x, int y);

    WlcsPointer* (*create_pointer)(WlcsDisplayServer* server);
    WlcsTouch* (*create_touch)(WlcsDisplayServer* server);
};

#define WLCS_SERVER_INTEGRATION_VERSION 1

struct WlcsServerIntegration
{
    uint32_t version;

    WlcsDisplayServer* (*create_server)(int argc, char const** argv);
    void (*destroy_server)(WlcsDisplayServer* server);
};


#ifdef __cplusplus
}
#endif

#endif //WLCS_SERVER_H_
