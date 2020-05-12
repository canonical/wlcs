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
 *
 * Authored by: Christopher William Wold <william.wold@canonical.com>
 */

#ifndef WLCS_WL_PROXY_H_
#define WLCS_WL_PROXY_H_

#include "boost/throw_exception.hpp"
#include "boost/current_function.hpp"

namespace wlcs
{
class Client;

/// A wrapper for any wl_proxy type
/// NOTE: the destroyer must be specified because the individual destroyers do more than just wl_proxy_destroy
template<typename T>
class WlProxy
{
public:
    WlProxy(T* const proxy, void(* const destroy)(T*))
        : proxy{proxy},
          destroy{destroy}
    {
    }

    WlProxy(WlProxy&&) = default;

    ~WlProxy()
    {
        destroy(proxy);
    }

    WlProxy(WlProxy const&) = delete;
    auto operator=(WlProxy const&) -> bool = delete;

    operator T*() const
    {
        return proxy;
    }

    auto wl_proxy() const -> struct wl_proxy*
    {
        return static_cast<struct wl_proxy*>(proxy);
    }

private:
    T* const proxy;
    void(* const destroy)(T*);
};

template<typename T>
auto wrap_proxy(T* const proxy, void(* const destroy)(T*)) -> WlProxy<T>
{
    return WlProxy<T>{proxy, destroy};
}
}

#endif // WLCS_WL_PROXY_H_
