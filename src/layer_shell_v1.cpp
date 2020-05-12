/*
 * Copyright © 2018-2019 Canonical Ltd.
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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "layer_shell_v1.h"

wlcs::LayerSurfaceV1::LayerSurfaceV1(
    wlcs::Client& client,
    wlcs::Surface& surface,
    zwlr_layer_shell_v1_layer layer,
    wl_output* output,
    const char *_namespace)
    : client{client},
      layer_shell{client},
      layer_surface{
          zwlr_layer_shell_v1_get_layer_surface(
          layer_shell,
          surface,
          output,
          layer,
          _namespace)}
{
    static struct zwlr_layer_surface_v1_listener const listener {
        [](void* data,
            struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
            uint32_t serial,
            uint32_t width,
            uint32_t height)
            {
                auto self = static_cast<LayerSurfaceV1*>(data);
                self->last_width_ = (int)width;
                self->last_height_ = (int)height;
                self->configure_count++;
                (void)zwlr_layer_surface_v1;
                (void)serial;
                zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);
            },
        [](void* /*data*/,
            struct zwlr_layer_surface_v1 */*zwlr_layer_surface_v1*/)
            {
            }
    };
    zwlr_layer_surface_v1_add_listener(layer_surface, &listener, this);
}

void wlcs::LayerSurfaceV1::dispatch_until_configure()
{
    client.dispatch_until([prev_config_count = configure_count, &current_config_count = configure_count]
        {
            return current_config_count > prev_config_count;
        });
}
