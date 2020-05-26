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

#include "xdg_output_v1.h"
#include "wl_handle.h"
#include "version_specifier.h"
#include <boost/throw_exception.hpp>

struct wlcs::XdgOutputManagerV1::Impl
{
    Impl(Client& client)
        : client{client},
          manager{client.bind_if_supported<zxdg_output_manager_v1>(AnyVersion)}
    {
    }

    Client& client;
    WlHandle<zxdg_output_manager_v1> const manager;
};

wlcs::XdgOutputManagerV1::XdgOutputManagerV1(Client& client)
    : impl{std::make_unique<Impl>(client)}
{
}

wlcs::XdgOutputManagerV1::~XdgOutputManagerV1() = default;

wlcs::XdgOutputManagerV1::operator zxdg_output_manager_v1*() const
{
    return impl->manager;
}

auto wlcs::XdgOutputManagerV1::client() const -> Client&
{
    return impl->client;
}

struct wlcs::XdgOutputV1::Impl
{
    Impl(XdgOutputManagerV1& manager, size_t output_index)
        : output{zxdg_output_manager_v1_get_xdg_output(
              manager,
              manager.client().output_state(output_index).output)},
          version{zxdg_output_v1_get_version(output)}
    {
        zxdg_output_v1_add_listener(output, &Impl::listener, this);
    }

    ~Impl()
    {
        zxdg_output_v1_destroy(output);
    }

    zxdg_output_v1* const output;
    uint32_t const version;
    bool dirty{false};
    State _state;

    static zxdg_output_v1_listener const listener;
};

zxdg_output_v1_listener const wlcs::XdgOutputV1::Impl::listener = {
    [] /* logical_position */ (void *data, zxdg_output_v1 *, int32_t x, int32_t y)
    {
        auto const impl = static_cast<Impl*>(data);
        impl->_state.logical_position = std::make_pair(x, y);
        impl->dirty = true;
    },

    [] /* logical_size */ (void *data, zxdg_output_v1 *, int32_t width, int32_t height)
    {
        auto const impl = static_cast<Impl*>(data);
        impl->_state.logical_size = std::make_pair(width, height);
        impl->dirty = true;
    },

    [] /* done */ (void *data, zxdg_output_v1 *)
    {
        auto const impl = static_cast<Impl*>(data);
        if (impl->version < 3)
            impl->dirty = false;
    },

    [] /* name */ (void *data, zxdg_output_v1 *, const char *name)
    {
        auto const impl = static_cast<Impl*>(data);
        std::string str{name};
        impl->_state.name = str;
        impl->dirty = true;
    },

    [] /* description */ (void *data, zxdg_output_v1 *, const char *description)
    {
        auto const impl = static_cast<Impl*>(data);
        impl->_state.description = std::string{description};
        impl->dirty = true;
    },
};

wlcs::XdgOutputV1::XdgOutputV1(XdgOutputManagerV1& manager, size_t output_index)
    : impl{std::make_shared<Impl>(manager, output_index)}
{
    if (impl->version >= 3)
    {
        manager.client().add_output_done_notifier(output_index, [weak = std::weak_ptr<Impl>(impl)]()
            {
                if (auto const impl = weak.lock())
                {
                    impl->dirty = false;
                }
            });
    }
}

wlcs::XdgOutputV1::operator zxdg_output_v1*() const
{
    return impl->output;
}

auto wlcs::XdgOutputV1::state() -> State const&
{
    if (impl->dirty)
    {
        std::string done_event = (impl->version >= 3 ? "wl_output.done" : "zxdg_output_v1.done");
        BOOST_THROW_EXCEPTION(std::logic_error("State change was not finished with a " + done_event + " event"));
    }

    return impl->_state;
}
