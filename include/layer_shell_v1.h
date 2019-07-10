/*
 * Copyright © 2019 Canonical Ltd.
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

#ifndef WLCS_LAYER_SHELL_V1_H
#define WLCS_LAYER_SHELL_V1_H

#include "in_process_server.h"

// Because _someone_ *cough*ddevault*cough* thought it would be a great idea to name an argument "namespace"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define namespace _namespace
#include "generated/wlr-layer-shell-unstable-v1-client.h"
#undef namespace
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace wlcs
{

class LayerSurfaceV1
{
public:
    LayerSurfaceV1(
        wlcs::Client& client,
        wlcs::Surface& surface,
        zwlr_layer_shell_v1_layer layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        wl_output* output = NULL,
        const char *_namespace = "wlcs");
    LayerSurfaceV1(LayerSurfaceV1 const&) = delete;
    LayerSurfaceV1& operator=(LayerSurfaceV1 const&) = delete;
    ~LayerSurfaceV1();

    operator zwlr_layer_surface_v1*() const { return layer_surface; }

    void dispatch_until_configure();
    auto last_width() const -> int { return _last_width; }
    auto last_height() const -> int { return _last_height; }

private:
    zwlr_layer_surface_v1* layer_surface;
    wlcs::Client& client;
    int _last_width = -1;
    int _last_height = -1;
    int configure_count = 0;
};

}

#endif // WLCS_LAYER_SHELL_V1_H
