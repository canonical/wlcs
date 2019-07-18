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
        EXPECT_THAT(manager.toplevels()[0]->is_dirty(), Eq(false));
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

TEST_F(ForeignToplevelHandleTest, gets_title)
{
    std::string title = "Test Title @!\\-";

    xdg_toplevel_set_title(xdg_toplevel, title.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)toplevel().title(), Eq(true));
    EXPECT_THAT(toplevel().title().value(), Eq(title));
}

TEST_F(ForeignToplevelHandleTest, title_gets_updated)
{
    std::string title_a = "Test Title @!\\-";
    std::string title_b = "Title 2";

    xdg_toplevel_set_title(xdg_toplevel, title_a.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)toplevel().title(), Eq(true));
    ASSERT_THAT(toplevel().title().value(), Eq(title_a));

    xdg_toplevel_set_title(xdg_toplevel, title_b.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)toplevel().title(), Eq(true));
    EXPECT_THAT(toplevel().title().value(), Eq(title_b));
}

TEST_F(ForeignToplevelHandleTest, gets_app_id)
{
    std::string app_id = "fake.wlcs.app.id";

    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)toplevel().app_id(), Eq(true));
    EXPECT_THAT(toplevel().app_id().value(), Eq(app_id));
}

TEST_F(ForeignToplevelHandleTest, gets_maximized)
{
    xdg_toplevel_set_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().maximized(), Eq(true));

    xdg_toplevel_unset_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().maximized(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, gets_minimized)
{
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));

    xdg_toplevel_set_minimized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().maximized(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, gets_fullscreen)
{
    xdg_toplevel_set_fullscreen(xdg_toplevel, nullptr);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().fullscreen(), Eq(true));

    xdg_toplevel_unset_fullscreen(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().maximized(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, gets_activated)
{
    std::string app_id = "fake.wlcs.app.id";

    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel().activated(), Eq(true));

    wlcs::Surface other = client.create_visible_surface(w, h);
    client.roundtrip();

    ASSERT_THAT(manager.toplevels().size(), Eq(2u));
    EXPECT_THAT(manager.toplevels()[0]->app_id(), Eq(app_id));
    EXPECT_THAT(manager.toplevels()[0]->activated(), Eq(false));

    EXPECT_THAT(manager.toplevels()[1]->activated(), Eq(true));
}

// TODO: test setting fullscreen/maximized/etc from the handle
// TODO: test popup does not come up as toplevel
