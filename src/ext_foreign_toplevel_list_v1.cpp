/*
 * Copyright © Canonical Ltd.
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

#include "ext_foreign_toplevel_list_v1.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "wl_handle.h"

#include <boost/throw_exception.hpp>

#include <algorithm>
#include <stdexcept>

struct wlcs::ExtForeignToplevelHandleV1::Impl
{
    Impl(ext_foreign_toplevel_handle_v1* handle);

    WlHandle<ext_foreign_toplevel_handle_v1> const handle;
    bool dirty{false};
    bool closed{false};
    std::optional<std::string> title;
    std::optional<std::string> app_id;
    std::optional<std::string> identifier;
};

wlcs::ExtForeignToplevelHandleV1::Impl::Impl(ext_foreign_toplevel_handle_v1* handle)
    : handle{handle}
{
    static ext_foreign_toplevel_handle_v1_listener const listener = {
        .closed = [](
            void* data,
            ext_foreign_toplevel_handle_v1*)
            {
                auto self = static_cast<Impl*>(data);
                self->closed = true;
                self->dirty = false;
            },
        .done = [](
            void* data,
            ext_foreign_toplevel_handle_v1*)
            {
                auto self = static_cast<Impl*>(data);
                self->dirty = false;
            },
        .title = [](
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* title)
            {
                auto self = static_cast<Impl*>(data);
                self->title = title;
                self->dirty = true;
            },
        .app_id = [](
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* app_id)
            {
                auto self = static_cast<Impl*>(data);
                self->app_id = app_id;
                self->dirty = true;
            },
        .identifier = [](
            void* data,
            ext_foreign_toplevel_handle_v1*,
            char const* identifier)
            {
                auto self = static_cast<Impl*>(data);
                self->identifier = identifier;
                self->dirty = true;
            },
    };
    ext_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

wlcs::ExtForeignToplevelHandleV1::ExtForeignToplevelHandleV1(
    ext_foreign_toplevel_handle_v1 *handle)
    : impl{std::make_unique<Impl>(handle)}
{
}

wlcs::ExtForeignToplevelHandleV1::~ExtForeignToplevelHandleV1() = default;

auto wlcs::ExtForeignToplevelHandleV1::is_dirty() const -> bool
{
    return impl->dirty;
}

auto wlcs::ExtForeignToplevelHandleV1::closed() const -> bool
{
    return impl->closed;
}

auto wlcs::ExtForeignToplevelHandleV1::title() const -> std::optional<std::string>
{
    return impl->title;
}

auto wlcs::ExtForeignToplevelHandleV1::app_id() const -> std::optional<std::string>
{
    return impl->app_id;
}

auto wlcs::ExtForeignToplevelHandleV1::identifier() const -> std::optional<std::string>
{
    return impl->identifier;
}

wlcs::ExtForeignToplevelHandleV1::operator ext_foreign_toplevel_handle_v1*() const
{
    return impl->handle;
}

struct wlcs::ExtForeignToplevelListV1::Impl {
    Impl(Client& client);

    WlHandle<ext_foreign_toplevel_list_v1> const list;
    std::vector<std::unique_ptr<ExtForeignToplevelHandleV1>> toplevels;
};

wlcs::ExtForeignToplevelListV1::Impl::Impl(Client& client)
    : list{client.bind_if_supported<ext_foreign_toplevel_list_v1>(wlcs::AnyVersion)}
{
    static ext_foreign_toplevel_list_v1_listener const listener = {
        .toplevel = [](
            void* data,
            ext_foreign_toplevel_list_v1*,
            ext_foreign_toplevel_handle_v1* toplevel)
            {
                auto self = static_cast<Impl*>(data);
                auto handle = std::make_unique<ExtForeignToplevelHandleV1>(toplevel);
                self->toplevels.push_back(std::move(handle));
            },
        .finished = [](
            void* /*data*/,
            ext_foreign_toplevel_list_v1 *)
            {
            },
    };
    ext_foreign_toplevel_list_v1_add_listener(list, &listener, this);
}

wlcs::ExtForeignToplevelListV1::ExtForeignToplevelListV1(Client& client)
    : impl{std::make_unique<Impl>(client)}
{
}

wlcs::ExtForeignToplevelListV1::~ExtForeignToplevelListV1() = default;

auto wlcs::ExtForeignToplevelListV1::toplevels() const -> std::vector<std::unique_ptr<ExtForeignToplevelHandleV1>> const&
{
    return impl->toplevels;
}

auto wlcs::ExtForeignToplevelListV1::toplevel() const -> ExtForeignToplevelHandleV1 const&
{
    if (impl->toplevels.empty())
        BOOST_THROW_EXCEPTION(std::runtime_error("Manager does not know about any toplevels"));
    if (impl->toplevels.size() > 1u)
        BOOST_THROW_EXCEPTION(std::runtime_error(
            "Manager knows about " + std::to_string(impl->toplevels.size()) + " toplevels"));
    if (impl->toplevels[0]->is_dirty())
        BOOST_THROW_EXCEPTION(std::runtime_error("Toplevel has pending updates"));
    return *impl->toplevels[0];
}

auto wlcs::ExtForeignToplevelListV1::toplevel(std::string const& app_id) const -> ExtForeignToplevelHandleV1 const&
{
    std::optional<ExtForeignToplevelHandleV1 const*> match;

    for (auto const& i : impl->toplevels)
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
        BOOST_THROW_EXCEPTION(std::out_of_range("No toplevels have the app ID " + app_id));
    if (match.value()->is_dirty())
        BOOST_THROW_EXCEPTION(std::runtime_error("Toplevel has pending updates"));
    return *match.value();
}

void wlcs::ExtForeignToplevelListV1::remove(ExtForeignToplevelHandleV1 const& toplevel)
{
    impl->toplevels.erase(std::remove_if(impl->toplevels.begin(), impl->toplevels.end(),
        [&toplevel](auto const& item)
            {
                return item.get() == &toplevel;
            }), impl->toplevels.end());
}
