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

#include <string>
#include <stdexcept>

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

    WlProxy()
        : proxy{nullptr},
          destroy{nullptr}
    {
    }

    WlProxy(Client const& /*client*/)
        : WlProxy()
    {
        std::string const current_function = BOOST_CURRENT_FUNCTION;
        BOOST_THROW_EXCEPTION(std::runtime_error(
            "WlProxy constructed with invalid global type. "
            "Add " + current_function + " specialization to wl_proxy.cpp"));
    }

    WlProxy(T* /*proxy*/)
        : WlProxy()
    {
        std::string const current_function = BOOST_CURRENT_FUNCTION;
        BOOST_THROW_EXCEPTION(std::runtime_error(
            "WlProxy constructed with invalid object type. "
            "Add " + current_function + " specialization to wl_proxy.cpp"));
    }

    WlProxy(WlProxy&&) = default;

    ~WlProxy()
    {
        if (destroy && proxy)
            destroy(proxy);
    }

    WlProxy(WlProxy const&) = delete;
    auto operator=(WlProxy const&) -> bool = delete;

    operator bool() const
    {
        return proxy != nullptr;
    }

    operator T*() const
    {
        if (!proxy)
            BOOST_THROW_EXCEPTION(std::runtime_error("Attempted to use null WlProxy"));
        return proxy;
    }

    auto wl_proxy() const -> struct wl_proxy*
    {
        if (!proxy)
            BOOST_THROW_EXCEPTION(std::runtime_error("Attempted to get proxy from null WlProxy"));
        return static_cast<struct wl_proxy*>(proxy);
    }

private:
    T* const proxy;
    void(* const destroy)(T*);
};
}

#endif // WLCS_WL_PROXY_H_
