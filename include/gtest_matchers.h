/*
 * Copyright © 2017-2019 Canonical Ltd.
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "in_process_server.h"

MATCHER_P2(IsSurfaceOfSize, width, height, "")
{
    auto& surface = const_cast<wlcs::Surface&>(arg);
    auto& client = arg.owner();
    auto& server = client.owner();

    server.move_surface_to(surface, 100, 100);

    auto pointer = server.create_pointer();

    // Use a shared ptr to extend the lifetime of the variables until the
    // pointer/motion notifications are done with them. Otherwise, we get a
    // use-after-free.
    auto const pointer_entered = std::make_shared<bool>(false);
    auto const motion_received = std::make_shared<bool>(false);
    client.add_pointer_enter_notification(
        [&surface, pointer_entered](auto entered_surface, auto, auto)
        {
            *pointer_entered = (entered_surface == surface);
            return false;
        });

    // First ensure we are *not* on the surface...
    pointer.move_to(0, 0);
    // ...then move onto the surface, so our enter notification fires
    pointer.move_to(100, 100);
    client.dispatch_until([pointer_entered]() { return *pointer_entered; });

    // Should be on the top left of the surface
    if (client.window_under_cursor() != surface)
    {
        *result_listener << "Surface at unexpected location (test harness bug?)";
        return false;
    }
    if (client.pointer_position() != std::make_pair(wl_fixed_from_int(0), wl_fixed_from_int(0)))
    {
        BOOST_THROW_EXCEPTION((std::runtime_error{"Surface at unexpected location (test harness bug?)"}));
    }

    client.add_pointer_motion_notification(
        [motion_received](auto, auto)
        {
            *motion_received = true;
            return false;
        });
    client.add_pointer_leave_notification(
        [pointer_entered](auto)
        {
            *pointer_entered = false;
            return false;
        });

    pointer.move_by(width - 1, height - 1);
    client.dispatch_until([&]() { return *motion_received || !*pointer_entered; });

    // Should now be at bottom corner of surface
    if (client.window_under_cursor() != surface)
    {
        *result_listener << "Surface smaller than (" << width << "×" << height << ")";
        ADD_FAILURE() << "Surface size too small";
        return false;
    }
    if (client.pointer_position() != std::make_pair(wl_fixed_from_int(width - 1), wl_fixed_from_int(height - 1)))
    {
        auto const [x, y] = client.pointer_position();
        *result_listener << "Surface coordinate system incorrect; expected (" << width - 1 << ", " << height - 1 << ") got ("
            << wl_fixed_to_double(x) << ", " << wl_fixed_to_double(y) << ")";
        return false;
    }

    // Moving any further should take us out of the surface
    *motion_received = false;

    client.add_pointer_leave_notification(
        [pointer_entered](auto)
        {
            *pointer_entered = false;
            return false;
        });

    client.add_pointer_motion_notification(
        [motion_received](auto, auto)
        {
            *motion_received = true;
            return false;
        });

    pointer.move_by(1, 1);
    client.dispatch_until([&]() { return !*pointer_entered || *motion_received; });

    if (client.window_under_cursor() == surface)
    {
        *result_listener << "Surface too large";
        return false;
    }

    return true;
}
