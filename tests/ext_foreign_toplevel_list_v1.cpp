/*
 * Copyright © 2025 Canonical Ltd.
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

#include "version_specifier.h"
#include "in_process_server.h"
#include "xdg_shell_stable.h"
#include "ext_foreign_toplevel_list_v1.h"

#include <gmock/gmock.h>

#include <algorithm>

using namespace testing;


namespace
{
int const w = 10, h = 15;

struct Window {
    Window(wlcs::Client& client)
        : surface{client},
          xdg_surface{client, surface},
          xdg_toplevel{xdg_surface}
    {
    }

    wlcs::Surface surface;
    wlcs::XdgSurfaceStable xdg_surface;
    wlcs::XdgToplevelStable xdg_toplevel;
};

class ExtForeignToplevelListTest
    : public wlcs::StartedInProcessServer
{
public:
    ExtForeignToplevelListTest()
        : client{the_server()}
    {
    }

    wlcs::Client client;
};

}

TEST_F(ExtForeignToplevelListTest, does_not_detect_toplevels_when_test_creates_none)
{
    wlcs::ExtForeignToplevelListV1 list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(0u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_from_same_client)
{
    auto const surface{client.create_visible_surface(w, h)};

    wlcs::ExtForeignToplevelListV1 list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_from_different_client)
{
    wlcs::Client observer_client{the_server()};

    auto const surface{client.create_visible_surface(w, h)};

    wlcs::ExtForeignToplevelListV1 list{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_created_after_list)
{
    wlcs::ExtForeignToplevelListV1 list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(0u));

    auto const surface{client.create_visible_surface(w, h)};

    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_multiple_toplevels_from_multiple_clients)
{
    wlcs::Client observer_client{the_server()};

    auto const surface{client.create_visible_surface(w, h)};
    auto const observer_surface{observer_client.create_visible_surface(w, h)};

    wlcs::ExtForeignToplevelListV1 list{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(2u));
}

TEST_F(ExtForeignToplevelListTest, handle_gets_title)
{
    std::string const title = "Test Title @!\\-";

    wlcs::ExtForeignToplevelListV1 list{client};
    Window win{client};
    xdg_toplevel_set_title(win.xdg_toplevel, title.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().title(), Eq(true));
    EXPECT_THAT(list.toplevel().title().value(), Eq(title));
}

TEST_F(ExtForeignToplevelListTest, title_gets_updated)
{
    std::string const title_a = "Test Title @!\\-";
    std::string const title_b = "Title 2";

    wlcs::ExtForeignToplevelListV1 list{client};
    Window win{client};
    xdg_toplevel_set_title(win.xdg_toplevel, title_a.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().title(), Eq(true));
    ASSERT_THAT(list.toplevel().title().value(), Eq(title_a));

    xdg_toplevel_set_title(win.xdg_toplevel, title_b.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().title(), Eq(true));
    EXPECT_THAT(list.toplevel().title().value(), Eq(title_b));
}

TEST_F(ExtForeignToplevelListTest, handle_gets_app_id)
{
    std::string const app_id = "fake.wlcs.app.id";

    wlcs::ExtForeignToplevelListV1 list{client};
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().app_id(), Eq(true));
    EXPECT_THAT(list.toplevel().app_id().value(), Eq(app_id));
}

TEST_F(ExtForeignToplevelListTest, handle_gets_identifier)
{
    wlcs::ExtForeignToplevelListV1 list{client};
    Window win{client};
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().identifier(), Eq(true));
    EXPECT_THAT(list.toplevel().identifier().value(), Not(Eq(std::string(""))));
}

TEST_F(ExtForeignToplevelListTest, identifiers_stable_across_lists)
{
    wlcs::ExtForeignToplevelListV1 list{client};

    // Create and destroy a few windows to ensure we're not just
    // testing the first window gets the same identifier.
    {
        Window win1{client};
        Window win2{client};
        win1.surface.attach_visible_buffer(w, h);
        win2.surface.attach_visible_buffer(w, h);
        client.roundtrip();
    }

    std::string const app_id = "fake.wlcs.app.id";
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel(app_id).identifier(), Eq(true));
    auto const identifier = list.toplevel(app_id).identifier().value();

    wlcs::ExtForeignToplevelListV1 list2{client};
    client.roundtrip();
    ASSERT_THAT(list2.toplevels().size(), Eq(1u));
    EXPECT_THAT(list2.toplevels()[0]->identifier(), Eq(identifier));
}

TEST_F(ExtForeignToplevelListTest, identifiers_stable_across_clients)
{
    wlcs::ExtForeignToplevelListV1 list{client};

    // Create and destroy a few windows to ensure we're not just
    // testing the first window gets the same identifier.
    {
        Window win1{client};
        Window win2{client};
        win1.surface.attach_visible_buffer(w, h);
        win2.surface.attach_visible_buffer(w, h);
        client.roundtrip();
    }

    std::string const app_id = "fake.wlcs.app.id";
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel(app_id).identifier(), Eq(true));
    auto const identifier = list.toplevel(app_id).identifier().value();

    wlcs::Client client2{the_server()};
    wlcs::ExtForeignToplevelListV1 list2{client2};
    client2.roundtrip();
    ASSERT_THAT(list2.toplevels().size(), Eq(1u));
    EXPECT_THAT(list2.toplevels()[0]->identifier(), Eq(identifier));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_closed)
{
    wlcs::ExtForeignToplevelListV1 list{client};
    client.roundtrip();
    EXPECT_THAT(list.toplevels().size(), Eq(0u));

    {
        Window win{client};
        win.surface.attach_visible_buffer(w, h);
        client.roundtrip();

        EXPECT_THAT(list.toplevels().size(), Eq(1u));
    }

    client.roundtrip();
    ASSERT_THAT(list.toplevel().closed(), Eq(true));
}

TEST_F(ExtForeignToplevelListTest, can_destroy_handles)
{
    std::string const title_a = "Title A";
    std::string const title_b = "Title B";

    wlcs::ExtForeignToplevelListV1 list{client};
    Window win{client};
    xdg_toplevel_set_title(win.xdg_toplevel, title_a.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));

    list.remove(list.toplevel());
    client.roundtrip();

    // Handle for visible window is not recreated after destruction,
    // even with metadata changes.
    xdg_toplevel_set_title(win.xdg_toplevel, title_b.c_str());
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(0u));
}
