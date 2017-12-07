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

typedef struct WlcsDisplayServer WlcsDisplayServer;
typedef struct WlcsPointer WlcsPointer;

WlcsDisplayServer* wlcs_create_server(int argc, char const** argv) __attribute__((weak));
void wlcs_destroy_server(WlcsDisplayServer* server) __attribute__((weak));

void wlcs_server_start(WlcsDisplayServer* server) __attribute__((weak));
void wlcs_server_stop(WlcsDisplayServer* server) __attribute__((weak));

int wlcs_server_create_client_socket(WlcsDisplayServer* server) __attribute__((weak));

void wlcs_server_position_window_absolute (WlcsDisplayServer* server, wl_display* client, wl_surface* surface, int x, int y) __attribute__((weak));

WlcsPointer* wlcs_server_create_pointer(WlcsDisplayServer* server) __attribute__((weak));
void wlcs_destroy_pointer(WlcsPointer* pointer) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif //WLCS_SERVER_H_
