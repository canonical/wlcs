/*
 * Copyright © 2026 Canonical Ltd.
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
 */

#include "in_process_server.h"
#include "expect_protocol_error.h"
#include "xdg_shell_stable.h"

#include <gmock/gmock.h>

using namespace testing;

namespace
{
class WlSubcompositorTest : public wlcs::StartedInProcessServer
{
public:
    WlSubcompositorTest()
        : client{the_server()},
          parent{client.create_visible_surface(surface_width, surface_height)},
          surface{client}
    {
    }

    wlcs::Client client;
    wlcs::Surface parent;
    wlcs::Surface surface;

    static int const surface_width = 200;
    static int const surface_height = 200;
};
}

TEST_F(WlSubcompositorTest, get_subsurface_with_roleless_surface)
{
    auto const subsurface =
        wlcs::wrap_wl_object(wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent));
    client.roundtrip();
    // If no protocol error, it succeeded.
}

TEST_F(WlSubcompositorTest, get_subsurface_with_previous_child)
{
    // Add the subsurface role, then remove it.
    auto const subsurface = wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent);
    wl_subsurface_destroy(subsurface);
    client.roundtrip();

    // Re-use the same surface to add the subsurface role back.
    auto const subsurface2 =
        wlcs::wrap_wl_object(wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent));
    client.roundtrip();
    // If no protocol error, it succeeded.
}

TEST_F(WlSubcompositorTest, get_subsurface_on_a_surface_with_another_role_is_a_protocol_error)
{
    wlcs::XdgSurfaceStable xdg_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_surface};
    client.roundtrip();

    EXPECT_PROTOCOL_ERROR({
        wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent);
        client.roundtrip();
    }, &wl_subcompositor_interface, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE);
}
