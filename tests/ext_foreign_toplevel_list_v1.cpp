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
#include "generated/ext-foreign-toplevel-list-v1-client.h"

#include <boost/throw_exception.hpp>
#include <gmock/gmock.h>

#include <algorithm>

using namespace testing;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_toplevel_list_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_toplevel_handle_v1)
}

namespace
{
int const w = 10, h = 15;

class ForeignToplevelHandle
{
public:
    ForeignToplevelHandle(ext_foreign_toplevel_handle_v1* handle);
    ForeignToplevelHandle(ForeignToplevelHandle const&) = delete;
    ForeignToplevelHandle& operator=(ForeignToplevelHandle const&) = delete;

    auto is_dirty() const { return dirty_; }
    auto closed() const { return closed_; }
    auto title() const { return title_; }
    auto app_id() const { return app_id_; }
    auto identifier() const { return identifier_; }

    operator ext_foreign_toplevel_handle_v1*() const { return handle; }
    wlcs::WlHandle<ext_foreign_toplevel_handle_v1> const handle;

private:
    static auto get_self(void* data) -> ForeignToplevelHandle*
    {
        return static_cast<ForeignToplevelHandle*>(data);
    }

    bool dirty_{false};
    bool closed_{false};
    std::optional<std::string> title_;
    std::optional<std::string> app_id_;
    std::optional<std::string> identifier_;
};

ForeignToplevelHandle::ForeignToplevelHandle(ext_foreign_toplevel_handle_v1* handle)
    : handle{handle}
{
    static ext_foreign_toplevel_handle_v1_listener const listener = {
        [] /* closed */ (
            void* data,
            ext_foreign_toplevel_handle_v1*)
            {
                auto self = get_self(data);
                self->closed_ = true;
                self->dirty_ = false;
            },
        [] /* done */ (
            void* data,
            ext_foreign_toplevel_handle_v1*)
            {
                auto self = get_self(data);
                self->dirty_ = false;
            },
        [] /* title */ (
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* title)
            {
                auto self = get_self(data);
                self->title_ = title;
                self->dirty_ = true;
            },
        [] /* app_id */ (
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* app_id)
            {
                auto self = get_self(data);
                self->app_id_ = app_id;
                self->dirty_ = true;
            },
        [] /* identifier */ (
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* identifier)
            {
                auto self = get_self(data);
                self->identifier_ = identifier;
                self->dirty_ = true;
            },
    };
    ext_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

class ForeignToplevelList
{
public:
    ForeignToplevelList(wlcs::Client& client);

    auto toplevels() -> std::vector<std::unique_ptr<ForeignToplevelHandle>> const&
    {
        return toplevels_;
    }

    auto toplevel() const -> ForeignToplevelHandle const&;
    auto toplevel(std::string const& app_id) const -> ForeignToplevelHandle const&;
    void remove(ForeignToplevelHandle const& toplevel);

    wlcs::WlHandle<ext_foreign_toplevel_list_v1> const list;

private:
    std::vector<std::unique_ptr<ForeignToplevelHandle>> toplevels_;
};

ForeignToplevelList::ForeignToplevelList(wlcs::Client& client)
    : list{client.bind_if_supported<ext_foreign_toplevel_list_v1>(wlcs::AnyVersion)}
{
    static ext_foreign_toplevel_list_v1_listener const listener = {
        [] /* toplevel */ (
            void* data,
            ext_foreign_toplevel_list_v1*,
            ext_foreign_toplevel_handle_v1* toplevel)
            {
                auto self = static_cast<ForeignToplevelList*>(data);
                auto handle = std::make_unique<ForeignToplevelHandle>(toplevel);
                self->toplevels_.push_back(std::move(handle));
            },
        [] /* finished */ (
            void* /*data*/,
            ext_foreign_toplevel_list_v1 *)
            {
            },
    };
    ext_foreign_toplevel_list_v1_add_listener(list, &listener, this);
}

auto ForeignToplevelList::toplevel() const -> ForeignToplevelHandle const&
{
    if (toplevels_.empty())
        BOOST_THROW_EXCEPTION(std::runtime_error("Manager does not know about any toplevels"));
    if (toplevels_.size() > 1u)
        BOOST_THROW_EXCEPTION(std::runtime_error(
            "Manager knows about " + std::to_string(toplevels_.size()) + " toplevels"));
    if (toplevels_[0]->is_dirty())
        BOOST_THROW_EXCEPTION(std::runtime_error("Toplevel has pending updates"));
    return *toplevels_[0];
}

auto ForeignToplevelList::toplevel(std::string const& app_id) const -> ForeignToplevelHandle const&
{
    std::optional<ForeignToplevelHandle const*> match;

    for (auto const& i : toplevels_)
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

void ForeignToplevelList::remove(ForeignToplevelHandle const& toplevel)
{
    toplevels_.erase(std::remove_if(toplevels_.begin(), toplevels_.end(),
        [&toplevel](auto const& item)
            {
                return item.get() == &toplevel;
            }), toplevels_.end());
}

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
    ForeignToplevelList list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(0u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_from_same_client)
{
    auto surface{client.create_visible_surface(w, h)};

    ForeignToplevelList list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_from_different_client)
{
    wlcs::Client observer_client{the_server()};

    auto surface{client.create_visible_surface(w, h)};

    ForeignToplevelList list{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_created_after_list)
{
    ForeignToplevelList list{client};
    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(0u));

    auto surface{client.create_visible_surface(w, h)};

    client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(1u));
}

TEST_F(ExtForeignToplevelListTest, detects_multiple_toplevels_from_multiple_clients)
{
    wlcs::Client observer_client{the_server()};

    auto surface{client.create_visible_surface(w, h)};
    auto observer_surface{observer_client.create_visible_surface(w, h)};

    ForeignToplevelList list{observer_client};
    observer_client.roundtrip();
    ASSERT_THAT(list.toplevels().size(), Eq(2u));
}

TEST_F(ExtForeignToplevelListTest, handle_gets_title)
{
    std::string title = "Test Title @!\\-";

    ForeignToplevelList list{client};
    Window win{client};
    xdg_toplevel_set_title(win.xdg_toplevel, title.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().title(), Eq(true));
    EXPECT_THAT(list.toplevel().title().value(), Eq(title));
}

TEST_F(ExtForeignToplevelListTest, title_gets_updated)
{
    std::string title_a = "Test Title @!\\-";
    std::string title_b = "Title 2";

    ForeignToplevelList list{client};
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
    std::string app_id = "fake.wlcs.app.id";

    ForeignToplevelList list{client};
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().app_id(), Eq(true));
    EXPECT_THAT(list.toplevel().app_id().value(), Eq(app_id));
}

TEST_F(ExtForeignToplevelListTest, handle_gets_identifier)
{
    ForeignToplevelList list{client};
    Window win{client};
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel().identifier(), Eq(true));
    EXPECT_THAT(list.toplevel().identifier().value(), Not(Eq(std::string(""))));
}

TEST_F(ExtForeignToplevelListTest, identifiers_stable_across_lists)
{
    ForeignToplevelList list{client};

    // Create and destroy a few windows to ensure we're not just
    // testing the first window gets the same identifier.
    {
        Window win1{client};
        Window win2{client};
        win1.surface.attach_visible_buffer(w, h);
        win2.surface.attach_visible_buffer(w, h);
        client.roundtrip();
    }

    std::string app_id = "fake.wlcs.app.id";
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel(app_id).identifier(), Eq(true));
    auto identifier = list.toplevel(app_id).identifier().value();

    auto list2 = ForeignToplevelList{client};
    client.roundtrip();
    ASSERT_THAT(list2.toplevels().size(), Eq(1u));
    EXPECT_THAT(list2.toplevels()[0]->identifier(), Eq(identifier));
}

TEST_F(ExtForeignToplevelListTest, identifiers_stable_across_clients)
{
    ForeignToplevelList list{client};

    // Create and destroy a few windows to ensure we're not just
    // testing the first window gets the same identifier.
    {
        Window win1{client};
        Window win2{client};
        win1.surface.attach_visible_buffer(w, h);
        win2.surface.attach_visible_buffer(w, h);
        client.roundtrip();
    }

    std::string app_id = "fake.wlcs.app.id";
    Window win{client};
    xdg_toplevel_set_app_id(win.xdg_toplevel, app_id.c_str());
    win.surface.attach_visible_buffer(w, h);
    client.roundtrip();

    ASSERT_THAT((bool)list.toplevel(app_id).identifier(), Eq(true));
    auto identifier = list.toplevel(app_id).identifier().value();

    auto client2 = wlcs::Client{the_server()};
    auto list2 = ForeignToplevelList{client2};
    client2.roundtrip();
    ASSERT_THAT(list2.toplevels().size(), Eq(1u));
    EXPECT_THAT(list2.toplevels()[0]->identifier(), Eq(identifier));
}

TEST_F(ExtForeignToplevelListTest, detects_toplevel_closed)
{
    ForeignToplevelList list{client};
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

    ForeignToplevelList list{client};
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
