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
#include "gmock/gmock.h"
#include <gtest/gtest.h>

using testing::_;
using testing::NotNull;

using namespace wlcs;

namespace
{
struct XdgDecorationV1Test : StartedInProcessServer
{
    Client a_client{the_server()};
    Surface a_surface{a_client};
    XdgSurfaceStable xdg_surface{a_client, a_surface};
    XdgToplevelStable xdg_toplevel{xdg_surface};

    ZxdgDecorationManagerV1 manager{a_client};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    void TearDown() override
    {
        a_client.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
} // namespace

TEST_F(XdgDecorationV1Test, can_get_decoration) { EXPECT_THAT(decoration, NotNull()); }

TEST_F(XdgDecorationV1Test, configure_happy_path)
{
    EXPECT_CALL(decoration, configure(_)).Times(2);

    decoration.configure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    a_client.roundtrip();

    decoration.configure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    a_client.roundtrip();
}
