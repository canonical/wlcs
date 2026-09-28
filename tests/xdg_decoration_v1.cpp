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

#include "xdg_decoration_unstable_v1.h"

#include "expect_protocol_error.h"
#include "generated/xdg-decoration-unstable-v1-client.h"
#include "in_process_server.h"
#include "xdg_shell_stable.h"
#include "gmock/gmock.h"

#include <boost/throw_exception.hpp>
#include <gtest/gtest.h>

#include <optional>

using testing::_;
using testing::AtLeast;
using testing::Eq;
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
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    EXPECT_NO_THROW(a_client.roundtrip());
}

TEST_F(XdgDecorationV1Test, duplicate_decorations_throw_already_constructed)
{
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};
    ZxdgToplevelDecorationV1 duplicate_decoration{manager, xdg_toplevel};

    EXPECT_PROTOCOL_ERROR(
        { a_client.roundtrip(); },
        &zxdg_decoration_manager_v1_interface,
        ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED);
}

TEST_F(XdgDecorationV1Test, destroying_toplevel_before_decoration_throws_orphaned)
{
    auto* xdg_toplevel2 = new XdgToplevelStable{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, *xdg_toplevel2};
    delete xdg_toplevel2;

    EXPECT_PROTOCOL_ERROR(
        { a_client.roundtrip(); }, &zxdg_decoration_manager_v1_interface, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED);
}

TEST_F(XdgDecorationV1Test, set_mode_client_results_in_a_configure_event)
{
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);

    EXPECT_CALL(decoration, configure(_)).Times(AtLeast(1));

    a_client.roundtrip();
}

TEST_F(XdgDecorationV1Test, set_mode_server_results_in_a_configure_event)
{
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

    EXPECT_CALL(decoration, configure(_)).Times(AtLeast(1));

    a_client.roundtrip();
}

TEST_F(XdgDecorationV1Test, unset_mode_results_in_a_configure_event)
{
    XdgToplevelStable xdg_toplevel{xdg_surface};
    ZxdgToplevelDecorationV1 decoration{manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_unset_mode(decoration);

    EXPECT_CALL(decoration, configure(_)).Times(AtLeast(1));

    a_client.roundtrip();
}

TEST_F(XdgDecorationV1Test, mode_is_retained_across_destroy_without_commit)
{
    if (zxdg_decoration_manager_v1_get_version(manager) < 2)
    {
        GTEST_SKIP() << "Compositor only supports zxdg_decoration_manager_v1 version 1";
    }

    XdgToplevelStable xdg_toplevel{xdg_surface};
    std::optional<ZxdgToplevelDecorationV1> decoration{std::in_place, manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_set_mode(*decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    EXPECT_CALL(*decoration, configure(Eq(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE))).Times(AtLeast(1));
    a_client.roundtrip();

    // Destroy and immediately recreate the decoration object, with no
    // intervening wl_surface.commit.
    decoration.reset();
    decoration.emplace(manager, xdg_toplevel);
    zxdg_toplevel_decoration_v1_unset_mode(*decoration);

    // The previously-negotiated mode should be retained, not the compositor's
    // own baseline default.
    EXPECT_CALL(*decoration, configure(Eq(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE))).Times(AtLeast(1));

    a_client.roundtrip();
}

TEST_F(XdgDecorationV1Test, mode_resets_after_commit_with_no_decoration_attached)
{
    if (zxdg_decoration_manager_v1_get_version(manager) < 2)
    {
        GTEST_SKIP() << "Compositor only supports zxdg_decoration_manager_v1 version 1";
    }

    XdgToplevelStable xdg_toplevel{xdg_surface};
    std::optional<ZxdgToplevelDecorationV1> decoration{std::in_place, manager, xdg_toplevel};

    zxdg_toplevel_decoration_v1_set_mode(*decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    EXPECT_CALL(*decoration, configure(Eq(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE))).Times(AtLeast(1));
    a_client.roundtrip();

    decoration.reset();
    // A real, buffered commit (as any client would issue) with no decoration attached.
    a_surface.attach_buffer(100, 100);
    wl_surface_commit(a_surface);
    a_client.roundtrip();

    decoration.emplace(manager, xdg_toplevel);
    zxdg_toplevel_decoration_v1_unset_mode(*decoration);

    // A commit happened with no decoration object attached, so the retained
    // mode should have been forgotten; this falls back to the compositor's
    // own default (client-side, per this suite's default decoration strategy).
    EXPECT_CALL(*decoration, configure(Eq(ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE))).Times(AtLeast(1));

    a_client.roundtrip();
}
