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
#include <boost/throw_exception.hpp>

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
        if (manager.toplevels().empty())
            BOOST_THROW_EXCEPTION(std::runtime_error("Manager does not know about any toplevels"));
        if (manager.toplevels().size() > 1u)
            BOOST_THROW_EXCEPTION(std::runtime_error(
                "Manager knows about " + std::to_string(manager.toplevels().size()) + " toplevels"));
        if (manager.toplevels()[0]->is_dirty())
            BOOST_THROW_EXCEPTION(std::runtime_error("Toplevel has pending updates"));
        return *manager.toplevels()[0];
    }

    auto toplevel(std::string const& app_id) -> wlcs::ForeignToplevelHandle const&
    {
        std::experimental::optional<wlcs::ForeignToplevelHandle const*> match;
        for (auto const& i : manager.toplevels())
        {
            if (i->app_id() == app_id)
            {
                if (match)
                    BOOST_THROW_EXCEPTION(std::runtime_error("Multiple toplevels have the same app ID " + app_id));
                else
                    match = i.get();
            }
        }
        if (!match)
            BOOST_THROW_EXCEPTION(std::runtime_error("No toplevels have the app ID " + app_id));
        if (match.value()->is_dirty())
            BOOST_THROW_EXCEPTION(std::runtime_error("Toplevel has pending updates"));
        return *match.value();
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

    auto surface{client.create_visible_surface(w, h)};

    wlcs::ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_from_different_client)
{
    wlcs::Client foreign_client{the_server()};
    wlcs::Client observer_client{the_server()};

    auto surface{foreign_client.create_visible_surface(w, h)};

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

    auto surface{client.create_visible_surface(w, h)};

    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_multiple_toplevels_from_multiple_clients)
{
    wlcs::Client foreign_client{the_server()};
    wlcs::Client observer_client{the_server()};

    auto foreign_surface{foreign_client.create_visible_surface(w, h)};
    auto observer_surface{observer_client.create_visible_surface(w, h)};

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
    wl_surface_commit(surface);
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(true));
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

    std::string other_app_id = "other.app.id";
    wlcs::Surface other{client};
    wlcs::XdgSurfaceStable other_xdg{client, other};
    wlcs::XdgToplevelStable other_toplevel{other_xdg};
    xdg_toplevel_set_app_id(xdg_toplevel, other_app_id.c_str());
    other.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).activated(), Eq(false));
    EXPECT_THAT(toplevel(other_app_id).activated(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_maximize_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    xdg_toplevel.add_configure_notification(
        [&state](int32_t width, int32_t height, struct wl_array *states)
        {
            state = wlcs::XdgToplevelStable::State{width, height, states};
        });
    xdg_toplevel_unset_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(false));

    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel());
    client.roundtrip();

    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(toplevel().maximized(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_unmaximize_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    xdg_toplevel.add_configure_notification(
        [&state](int32_t width, int32_t height, struct wl_array *states)
        {
            state = wlcs::XdgToplevelStable::State{width, height, states};
        });
    xdg_toplevel_set_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(toplevel().maximized(), Eq(true));

    zwlr_foreign_toplevel_handle_v1_unset_maximized(toplevel());
    client.roundtrip();

    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, can_fullscreen_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    xdg_toplevel.add_configure_notification(
        [&state](int32_t width, int32_t height, struct wl_array *states)
        {
            state = wlcs::XdgToplevelStable::State{width, height, states};
        });
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(toplevel().fullscreen(), Eq(false));

    zwlr_foreign_toplevel_handle_v1_set_fullscreen(toplevel(), nullptr);
    client.roundtrip();

    EXPECT_THAT(state.fullscreen, Eq(true));
    EXPECT_THAT(toplevel().fullscreen(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_unfullscreen_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    xdg_toplevel.add_configure_notification(
        [&state](int32_t width, int32_t height, struct wl_array *states)
        {
            state = wlcs::XdgToplevelStable::State{width, height, states};
        });
    xdg_toplevel_set_fullscreen(xdg_toplevel, nullptr);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(state.fullscreen, Eq(true));
    EXPECT_THAT(toplevel().fullscreen(), Eq(true));

    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel());
    client.roundtrip();

    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(toplevel().fullscreen(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, can_minimize_foreign)
{
    std::string app_id = "fake.wlcs.app.id";

    wlcs::Surface below_surface{client.create_visible_surface(w, h)};
    the_server().move_surface_to(below_surface, 0, 0);
    client.roundtrip();

    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    the_server().move_surface_to(surface, 0, 0);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(1, 1);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(false));
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)surface));

    zwlr_foreign_toplevel_handle_v1_set_minimized(toplevel());
    client.roundtrip();

    pointer.move_to(2, 2);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(true));
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)below_surface));
}

TEST_F(ForeignToplevelHandleTest, can_unminimize_foreign)
{
    std::string app_id = "fake.wlcs.app.id";

    wlcs::Surface below_surface{client.create_visible_surface(w, h)};
    the_server().move_surface_to(below_surface, 0, 0);

    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    the_server().move_surface_to(surface, 0, 0);
    client.roundtrip();

    xdg_toplevel_set_minimized(xdg_toplevel);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(1, 1);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(true));
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)below_surface));

    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel());
    client.roundtrip();

    pointer.move_to(2, 2);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(false));
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)surface));
}

TEST_F(ForeignToplevelHandleTest, can_activate_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    xdg_toplevel.add_configure_notification(
        [&state](int32_t width, int32_t height, struct wl_array *states)
        {
            state = wlcs::XdgToplevelStable::State{width, height, states};
        });
    std::string app_id = "fake.wlcs.app.id";
    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    std::string other_app_id = "other.app.id";
    wlcs::Surface other{client};
    wlcs::XdgSurfaceStable other_xdg{client, other};
    wlcs::XdgToplevelStable other_toplevel{other_xdg};
    xdg_toplevel_set_app_id(xdg_toplevel, other_app_id.c_str());
    other.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(state.activated, Eq(false));
    EXPECT_THAT(toplevel(app_id).activated(), Eq(false));
    EXPECT_THAT(toplevel(other_app_id).activated(), Eq(true));

    zwlr_foreign_toplevel_handle_v1_activate(toplevel(app_id), client.seat());
    client.roundtrip();

    EXPECT_THAT(state.activated, Eq(true));
    EXPECT_THAT(toplevel(app_id).activated(), Eq(true));
    EXPECT_THAT(toplevel(other_app_id).activated(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, can_close_foreign)
{
    bool should_close{false};
    xdg_toplevel.add_close_notification(
        [&should_close]()
        {
            should_close = true;
        });
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(should_close, Eq(false));

    zwlr_foreign_toplevel_handle_v1_close(toplevel());
    client.roundtrip();

    EXPECT_THAT(should_close, Eq(true));
}

// TODO: test toplevel outputs
// TODO: test fullscreening toplevel on a specific output
// TODO: test popup does not come up as toplevel
// TODO: test zwlr_foreign_toplevel_handle_v1_set_rectangle somehow
