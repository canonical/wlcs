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

#include "wl_interface_descriptor.h"
#include "boost/throw_exception.hpp"
#include "boost/current_function.hpp"

#include <string>
#include <stdexcept>

struct wl_proxy;

namespace wlcs
{
template<typename T>
class WlHandle
{
public:
    explicit WlHandle(T* const proxy)
        : proxy{proxy},
          owns_wl_object{true}
    {
        if (proxy == nullptr)
        {
            BOOST_THROW_EXCEPTION((std::logic_error{"Attempt to construct a WlHandle from null Wayland object"}));
        }
    }

    ~WlHandle()
    {
        if (owns_wl_object)
        {
            (*WlInterfaceDescriptor<T>::destructor)(proxy);
        }
    }

    WlHandle(WlHandle&& from)
        : WlHandle(from.proxy)
    {
        from.owns_wl_object = false;
    }

    WlHandle(WlHandle const&) = delete;

    auto operator=(WlHandle&&) -> WlHandle& = delete;
    auto operator=(WlHandle const&) -> bool = delete;

    operator T*() const
    {
        // This is a precondition failure, but as this is a test-suite lets be generous and make it fail hard and fast
        if (!owns_wl_object)
        {
            std::abort();
        }
        return proxy;
    }

    auto wl_proxy() const -> struct wl_proxy*
    {
        // This is a precondition failure, but as this is a test-suite lets be generous and make it fail hard and fast
        if (!owns_wl_object)
        {
            std::abort();
        }
        return static_cast<struct wl_proxy*>(proxy);
    }

private:
    T* const proxy;
    bool owns_wl_object;
};

template<typename WlType>
auto wrap_wl_object(WlType* proxy) -> WlHandle<WlType>
{
    static_assert(
        WlInterfaceDescriptor<WlType>::interface != nullptr,
        "Missing specialisation for WlInterfaceDescriptor");
    return WlHandle<WlType>{proxy};
}

}

#endif // WLCS_WL_PROXY_H_
