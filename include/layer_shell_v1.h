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

#ifndef WLCS_LAYER_SHELL_V1_H
#define WLCS_LAYER_SHELL_V1_H

#include "in_process_server.h"
#include "wl_handle.h"
#include "geometry/size.h"

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

// We need to use a custom destructor as .destroyed() changed in v3
namespace
{
void send_destroy_if_supported(zwlr_layer_shell_v1* to_destroy)
{
    if (zwlr_layer_shell_v1_get_version(to_destroy) >= ZWLR_LAYER_SHELL_V1_DESTROY_SINCE_VERSION)
    {
        zwlr_layer_shell_v1_destroy(to_destroy);
    }
    else
    {
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(to_destroy));
    }
}
}
template<>
struct WlInterfaceDescriptor<zwlr_layer_shell_v1>
{
    static constexpr bool const has_specialisation = true;
    static constexpr wl_interface const* const interface = &zwlr_layer_shell_v1_interface;
    static constexpr void (* const destructor)(zwlr_layer_shell_v1*) = &send_destroy_if_supported;
};

WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_layer_surface_v1)

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

    operator zwlr_layer_surface_v1*() const { return layer_surface; }
    operator zwlr_layer_shell_v1*() const { return layer_shell; }

    void dispatch_until_configure();
    auto last_size() const -> wlcs::Size { return last_size_; }

private:
    wlcs::Client& client;
    WlHandle<zwlr_layer_shell_v1> layer_shell;
    WlHandle<zwlr_layer_surface_v1> layer_surface;
    Size last_size_ = {-1, -1};
    int configure_count = 0;
};

}

#endif // WLCS_LAYER_SHELL_V1_H
