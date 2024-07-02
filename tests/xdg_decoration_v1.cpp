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

#include "generated/xdg-decoration-unstable-v1-client.h"
#include "in_process_server.h"
#include "xdg_decoration_unstable_v1.h"
#include "xdg_shell_stable.h"
#include "gmock/gmock.h"
#include <boost/throw_exception.hpp>
#include <gtest/gtest.h>

using testing::_;
using namespace wlcs;

class XdgDecorationV1Test : public StartedInProcessServer
{
public:
    Client a_client{the_server()};
    ZxdgDecorationManagerV1 manager{a_client};
    Surface a_surface{a_client};
    XdgSurfaceStable xdg_surface{a_client, a_surface};
};

TEST_F(XdgDecorationV1Test, happy_path)
{
    {
        XdgToplevelStable xdg_toplevel{xdg_surface};
        ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};
    }

    EXPECT_NO_THROW(a_client.roundtrip());
}

TEST_F(XdgDecorationV1Test, already_constructed)
{
    {
        XdgToplevelStable xdg_toplevel{xdg_surface};
        ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};
        ZxdgToplevelDecorationV1 duplicate_decoration{manager, xdg_toplevel};
    }

    EXPECT_THROW(a_client.roundtrip(), wlcs::ProtocolError);
}

TEST_F(XdgDecorationV1Test, destroy_toplevel_before_decoration)
{
    auto* xdg_toplevel2 = new XdgToplevelStable{xdg_surface};

    {
        ZxdgToplevelDecorationV1 decoration{manager, *xdg_toplevel2};
        delete xdg_toplevel2;
    }

    EXPECT_THROW(a_client.roundtrip(), wlcs::ProtocolError);
}

TEST_F(XdgDecorationV1Test, set_and_unset_mode)
{
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    zxdg_toplevel_decoration_v1_unset_mode(decoration);

    EXPECT_CALL(decoration, configure(_)).Times(3);

    a_client.roundtrip();
}
