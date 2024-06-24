/*
 * Copyright © 2020 Canonical Ltd.
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
#include "xdg_decoration_unstable_v1.h"
#include "xdg_shell_stable.h"
#include <gtest/gtest.h>

using namespace wlcs;

using XdgDecorationV1Test = StartedInProcessServer;

TEST_F(XdgDecorationV1Test, happy_path)
{
    Client a_client{the_server()};
    Surface a_surface{a_client};
    XdgSurfaceStable xdg_surface{a_client, a_surface};
    XdgToplevelStable xdg_toplevel{xdg_surface};

    ZxdgDecorationManagerV1 manager{a_client};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    EXPECT_NO_THROW(a_client.roundtrip());
}

TEST_F(XdgDecorationV1Test, already_constructed)
{
    Client a_client{the_server()};
    Surface a_surface{a_client};
    XdgSurfaceStable xdg_surface{a_client, a_surface};
    XdgToplevelStable xdg_toplevel{xdg_surface};

    ZxdgDecorationManagerV1 manager{a_client};

    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};
    ZxdgToplevelDecorationV1 duplicate_decoration{manager, xdg_toplevel};

    EXPECT_THROW(a_client.roundtrip(), wlcs::ProtocolError);
}
