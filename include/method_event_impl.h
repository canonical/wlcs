/*
 * Copyright © 2021 Canonical Ltd.
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

#ifndef WLCS_METHOD_EVENT_IMPL_H_
#define WLCS_METHOD_EVENT_IMPL_H_

#include "wl_handle.h"

namespace wlcs
{
namespace detail
{
template<typename MemberFn>
struct MemberFunctionClass;

template <typename Ret, typename T, typename... Args>
struct MemberFunctionClass<Ret (T::*)(Args...)> {
    using type = T;
};
}

/// Can be used to easily implement Wayland listeners using methods, including gmock methods
template<auto member_fn, typename WlType, typename... Args>
void method_event_impl(void* data, WlType*, Args... args)
{
    auto self = static_cast<typename detail::MemberFunctionClass<decltype(member_fn)>::type*>(data);
    (self->*member_fn)(args...);
}

}

#endif // WLCS_METHOD_EVENT_IMPL_H_
