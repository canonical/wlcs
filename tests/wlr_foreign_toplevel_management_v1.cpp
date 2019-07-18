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

#include "helpers.h"
#include "in_process_server.h"
#include "foreign_toplevel_management_v1.h"
#include "xdg_shell_stable.h"

#include <gmock/gmock.h>

using namespace testing;

namespace
{
int const w = 10, h = 15;

class ForeignToplevelManagerTest
    : public wlcs::StartedInProcessServer
{
};

class ForeignToplevelHandleTest
    : public wlcs::StartedInProcessServer
{
public:
    ForeignToplevelHandleTest()
        : client{the_server()},
          manager{client},
          surface{client},
          xdg_surface{client, surface},
          xdg_toplevel{xdg_surface}
    {
    }

    auto toplevel() -> wlcs::ForeignToplevelHandle const&
    {
        EXPECT_THAT(manager.toplevels().size(), Eq(1u));
        EXPECT_THAT(manager.toplevels()[0]->is_dirty(), IsFalse());
        return *manager.toplevels()[0];
    }

    wlcs::Client client;
    wlcs::ForeignToplevelManager manager;
    wlcs::Surface surface;
    wlcs::XdgSurfaceStable xdg_surface;
    wlcs::XdgToplevelStable xdg_toplevel;
};
}

TEST_F(ForeignToplevelManagerTest, does_not_detect_toplevels_when_test_creates_none)
{
    wlcs::Client client{the_server()};
    wlcs::ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(0u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_from_same_client)
{
    wlcs::Client client{the_server()};

    client.create_visible_surface(w, h);

    wlcs::ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_from_different_client)
{
    wlcs::Client foreign_client{the_server()};
    wlcs::Client observer_client{the_server()};

    foreign_client.create_visible_surface(w, h);

    wlcs::ForeignToplevelManager manager{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_created_after_manager)
{
    wlcs::Client client{the_server()};

    wlcs::ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(0u));

    client.create_visible_surface(w, h);

    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_multiple_toplevels_from_multiple_clients)
{
    wlcs::Client foreign_client{the_server()};
    wlcs::Client observer_client{the_server()};

    foreign_client.create_visible_surface(w, h);
    observer_client.create_visible_surface(w, h);

    wlcs::ForeignToplevelManager manager{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(2u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_closed)
{
    wlcs::Client client{the_server()};

    wlcs::ForeignToplevelManager manager{client};
    client.roundtrip();
    EXPECT_THAT(manager.toplevels().size(), Eq(0u));

    {
        wlcs::Surface other{client};
        wlcs::XdgSurfaceStable other_xdg{client, other};
        wlcs::XdgToplevelStable other_toplevel{other_xdg};
        other.attach_visible_buffer(w, h);
        client.roundtrip();

        EXPECT_THAT(manager.toplevels().size(), Eq(1u));
    }

    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(0u));
}
