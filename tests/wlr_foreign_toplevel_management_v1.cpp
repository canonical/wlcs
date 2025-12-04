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
#include "version_specifier.h"
#include "in_process_server.h"
#include "xdg_shell_stable.h"
#include "generated/wlr-foreign-toplevel-management-unstable-v1-client.h"

#include <gmock/gmock.h>
#include <boost/throw_exception.hpp>
#include <algorithm>

using namespace testing;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_foreign_toplevel_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_foreign_toplevel_handle_v1)
}

namespace
{
int const w = 100, h = 150;

class ForeignToplevelHandle
{
public:
    ForeignToplevelHandle(zwlr_foreign_toplevel_handle_v1* handle);
    ForeignToplevelHandle(ForeignToplevelHandle const&) = delete;
    ForeignToplevelHandle& operator=(ForeignToplevelHandle const&) = delete;

    auto is_dirty() const { return dirty_; }
    auto title() const { return title_; }
    auto app_id() const { return app_id_; }
    auto outputs() const -> std::vector<wl_output*> const& { return outputs_; }
    auto maximized() const { return maximized_; }
    auto minimized() const { return minimized_; }
    auto activated() const { return activated_; }
    auto fullscreen() const { return fullscreen_; }
    auto destroyed() const { return destroyed_; }

    operator zwlr_foreign_toplevel_handle_v1*() const { return handle; }
    wlcs::WlHandle<zwlr_foreign_toplevel_handle_v1> const handle;

private:
    static auto get_self(void* data) -> ForeignToplevelHandle*
    {
        return static_cast<ForeignToplevelHandle*>(data);
    }

    bool dirty_{false};
    std::optional<std::string> title_;
    std::optional<std::string> app_id_;
    std::vector<wl_output*> outputs_;
    bool maximized_{false}, minimized_{false}, activated_{false}, fullscreen_{false};

    bool destroyed_{false};
};

ForeignToplevelHandle::ForeignToplevelHandle(zwlr_foreign_toplevel_handle_v1* handle)
    : handle{handle}
{
    static zwlr_foreign_toplevel_handle_v1_listener const listener = {
        [] /* title */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            char const* title)
            {
                auto self = get_self(data);
                self->title_ = title;
                self->dirty_ = true;
            },
        [] /* app_id */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            char const* app_id)
            {
                auto self = get_self(data);
                self->app_id_ = app_id;
                self->dirty_ = true;
            },
        [] /* output_enter */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_output* output)
            {
                auto self = get_self(data);
                self->outputs_.push_back(output);
                self->dirty_ = true;
            },
        [] /* output_leave */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_output* output)
            {
                auto self = get_self(data);
                std::erase(self->outputs_, output);
                self->dirty_ = true;
            },
        [] /*state */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_array* state)
            {
                auto self = get_self(data);

                self->maximized_ = false;
                self->minimized_ = false;
                self->activated_ = false;
                self->fullscreen_ = false;

                for (auto item = static_cast<zwlr_foreign_toplevel_handle_v1_state*>(state->data);
                    (char*)item < static_cast<char*>(state->data) + state->size;
                    item++)
                {
                    switch (*item)
                    {
                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                        self->maximized_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                        self->minimized_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                        self->activated_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                        self->fullscreen_ = true;
                        break;
                    }
                }
                self->dirty_ = true;
            },
        [] /* done */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*)
            {
                auto self = get_self(data);
                self->dirty_ = false;
            },
        [] /* closed */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*)
            {
                auto self = get_self(data);
                self->destroyed_ = true;
                self->dirty_ = false;
            }
    };
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

class ForeignToplevelManager
{
public:
    ForeignToplevelManager(wlcs::Client& client);

    auto toplevels() -> std::vector<std::unique_ptr<ForeignToplevelHandle>> const&
    {
        toplevels_.erase(
            std::remove_if(
                toplevels_.begin(),
                toplevels_.end(),
                [](auto const& toplevel) { return toplevel->destroyed(); }),
            toplevels_.end());

        return toplevels_;
    }

    wlcs::WlHandle<zwlr_foreign_toplevel_manager_v1> const manager;

private:
    std::vector<std::unique_ptr<ForeignToplevelHandle>> toplevels_;
};

ForeignToplevelManager::ForeignToplevelManager(wlcs::Client& client)
    : manager{client.bind_if_supported<zwlr_foreign_toplevel_manager_v1>(wlcs::AnyVersion)}
{
    static zwlr_foreign_toplevel_manager_v1_listener const listener = {
        +[] /* toplevel */ (
            void* data,
            zwlr_foreign_toplevel_manager_v1*,
            zwlr_foreign_toplevel_handle_v1* toplevel)
            {
                auto self = static_cast<ForeignToplevelManager*>(data);
                auto handle = std::make_unique<ForeignToplevelHandle>(toplevel);
                self->toplevels_.push_back(std::move(handle));
            },
        +[] /* finished */ (
            void* /*data*/,
            zwlr_foreign_toplevel_manager_v1 *)
            {
            }
    };
    zwlr_foreign_toplevel_manager_v1_add_listener(manager, &listener, this);
}

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

    auto toplevel() -> ForeignToplevelHandle const&
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

    auto toplevel(std::string const& app_id) -> ForeignToplevelHandle const&
    {
        std::optional<ForeignToplevelHandle const*> match;
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
    ForeignToplevelManager manager;
    wlcs::Surface surface;
    wlcs::XdgSurfaceStable xdg_surface;
    wlcs::XdgToplevelStable xdg_toplevel;
};
}

TEST_F(ForeignToplevelManagerTest, does_not_detect_toplevels_when_test_creates_none)
{
    wlcs::Client client{the_server()};
    ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(0u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_from_same_client)
{
    wlcs::Client client{the_server()};

    auto surface{client.create_visible_surface(w, h)};

    ForeignToplevelManager manager{client};
    client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_from_different_client)
{
    wlcs::Client foreign_client{the_server()};
    wlcs::Client observer_client{the_server()};

    auto surface{foreign_client.create_visible_surface(w, h)};

    ForeignToplevelManager manager{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(1u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_created_after_manager)
{
    wlcs::Client client{the_server()};

    ForeignToplevelManager manager{client};
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

    ForeignToplevelManager manager{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(manager.toplevels().size(), Eq(2u));
}

TEST_F(ForeignToplevelManagerTest, detects_toplevel_closed)
{
    wlcs::Client client{the_server()};

    ForeignToplevelManager manager{client};
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
    xdg_toplevel_set_app_id(other_toplevel, other_app_id.c_str());
    other.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).activated(), Eq(false));
    EXPECT_THAT(toplevel(other_app_id).activated(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_maximize_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
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
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
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
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
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
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
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

    ASSERT_THAT(toplevel(app_id).minimized(), Eq(false)) << "precondition failed";
    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)surface)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_set_minimized(toplevel(app_id));
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(true));

    pointer.move_to(2, 2);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne((wl_surface*)surface))
        << "surface under pointer when it should have been minimized";
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)below_surface))
        << "surface under pointer not correct";
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

    ASSERT_THAT(toplevel(app_id).minimized(), Eq(true)) << "precondition failed";
    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)below_surface)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel(app_id));
    client.roundtrip();

    EXPECT_THAT(toplevel(app_id).minimized(), Eq(false));

    pointer.move_to(2, 2);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne((wl_surface*)below_surface))
        << "surface under pointer when it should have been occluded by unminimized surface";
    EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)surface))
        << "surface under pointer not correct";
}

TEST_F(ForeignToplevelHandleTest, can_unminimize_foreign_to_restored)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    surface.attach_visible_buffer(w, h);
    xdg_toplevel_unset_maximized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().maximized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel());
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(false));
    EXPECT_THAT(state.maximized, Eq(false));
}

TEST_F(ForeignToplevelHandleTest, can_unminimize_foreign_to_maximized)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    surface.attach_visible_buffer(w, h);
    xdg_toplevel_set_maximized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().maximized(), Eq(true)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel());
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(true));
    EXPECT_THAT(state.maximized, Eq(true));
}

TEST_F(ForeignToplevelHandleTest, gets_activated_when_unminimized)
{
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel());
    client.roundtrip();

    EXPECT_THAT(toplevel().activated(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, gets_unminimized_when_activated)
{
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_activate(toplevel(), client.seat());
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, gets_unminimized_when_maximized)
{
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel());
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, gets_unminimized_when_fullscreened)
{
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_minimized(xdg_toplevel);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(toplevel().minimized(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_set_fullscreen(toplevel(), nullptr);
    client.roundtrip();

    EXPECT_THAT(toplevel().minimized(), Eq(false));
    EXPECT_THAT(toplevel().fullscreen(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_unfullscreen_foreign_to_restored)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    xdg_toplevel_unset_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(state.maximized, Eq(false)) << "precondition failed";
    ASSERT_THAT(toplevel().maximized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_fullscreen(xdg_toplevel, nullptr);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel());
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(false)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(false)) << "precondition failed";

    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(toplevel().maximized(), Eq(false));
}

TEST_F(ForeignToplevelHandleTest, can_unfullscreen_foreign_to_maximized)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    xdg_toplevel_set_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(state.maximized, Eq(true)) << "precondition failed";
    ASSERT_THAT(toplevel().maximized(), Eq(true)) << "precondition failed";

    xdg_toplevel_set_fullscreen(xdg_toplevel, nullptr);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel());
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(false)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(false)) << "precondition failed";

    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(toplevel().maximized(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_maximize_foreign_while_fullscreen)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    xdg_toplevel_unset_maximized(xdg_toplevel);
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT(state.maximized, Eq(false)) << "precondition failed";
    ASSERT_THAT(toplevel().maximized(), Eq(false)) << "precondition failed";

    xdg_toplevel_set_fullscreen(xdg_toplevel, nullptr);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(true)) << "precondition failed";

    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel());
    client.roundtrip();

    EXPECT_THAT(state.fullscreen, Eq(true)) << "XDG toplevel became not fullscreen after requesting maximized";
    EXPECT_THAT(toplevel().fullscreen(), Eq(true)) << "foreign toplevel became not fullscreen after maximize";

    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel());
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(state.fullscreen, Eq(false)) << "precondition failed";
    ASSERT_THAT(toplevel().fullscreen(), Eq(false)) << "precondition failed";

    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(toplevel().maximized(), Eq(true));
}

TEST_F(ForeignToplevelHandleTest, can_activate_foreign)
{
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
    ON_CALL(xdg_toplevel, configure).WillByDefault([&state](auto... args)
        {
            state = wlcs::XdgToplevelStable::State{args...};
        });
    std::string app_id = "fake.wlcs.app.id";
    xdg_toplevel_set_app_id(xdg_toplevel, app_id.c_str());
    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    std::string other_app_id = "other.app.id";
    wlcs::Surface other{client};
    wlcs::XdgSurfaceStable other_xdg{client, other};
    wlcs::XdgToplevelStable other_toplevel{other_xdg};
    xdg_toplevel_set_app_id(other_toplevel, other_app_id.c_str());
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
    EXPECT_CALL(xdg_toplevel, close()).Times(0);

    surface.attach_visible_buffer(w, h);
    client.roundtrip();

    EXPECT_CALL(xdg_toplevel, close()).Times(1);

    zwlr_foreign_toplevel_handle_v1_close(toplevel());
    client.roundtrip();
}

// TODO: test toplevel outputs
// TODO: test fullscreening toplevel on a specific output
// TODO: test popup does not come up as toplevel
// TODO: test zwlr_foreign_toplevel_handle_v1_set_rectangle somehow
