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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "relative_pointer_unstable_v1.h"
#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{
auto const any_width = 300;
auto const any_height = 300;
auto const nw_middle_x = any_width / 2;
auto const nw_middle_y = any_height / 2;

struct RelativePointer : StartedInProcessServer
{
    // Create a client with a surface and a cursor centered on the surface
    Client a_client{the_server()};
    Surface a_surface{a_client.create_visible_surface(any_width, any_height)};
    Pointer cursor = the_server().create_pointer();

    ZwpRelativePointerManagerV1 manager{a_client};
    ZwpRelativePointerV1 pointer{manager, a_client.the_pointer()};

    void SetUp() override
    {
        StartedInProcessServer::SetUp();

        // Get the surface in a known position with the cursor over it
        the_server().move_surface_to(a_surface, 0, 0);
        cursor.move_to(nw_middle_x, nw_middle_y);
    }

    void TearDown() override
    {
        a_client.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(RelativePointer, can_get_relative_pointer)
{
    EXPECT_THAT(pointer, NotNull());
}

TEST_F(RelativePointer, relative_pointer_gets_movement)
{
    auto const move_x = any_width/6;
    auto const move_y = any_height/6;

    EXPECT_CALL(pointer, relative_motion(_, _,
                                         wl_fixed_from_int(move_x), wl_fixed_from_int(move_y),
                                         wl_fixed_from_int(move_x), wl_fixed_from_int(move_y))).Times(1);

    cursor.move_by(move_x, move_y);
}

namespace
{
struct PointerListener
{
    PointerListener(wl_pointer* pointer)
    {
        wl_pointer_add_listener(pointer, &thunks, this);
    }

    MOCK_METHOD5(enter, void(
        struct wl_pointer *wl_pointer,
        uint32_t serial,
        struct wl_surface *surface,
        wl_fixed_t surface_x,
        wl_fixed_t surface_y));

    MOCK_METHOD3(leave, void(
        struct wl_pointer *wl_pointer,
        uint32_t serial,
        struct wl_surface *surface));

    MOCK_METHOD4(motion, void(
        struct wl_pointer *wl_pointer,
        uint32_t time,
        wl_fixed_t surface_x,
        wl_fixed_t surface_y));

    MOCK_METHOD5(button, void(
        struct wl_pointer *wl_pointer,
        uint32_t serial,
        uint32_t time,
        uint32_t button,
        uint32_t state));

    MOCK_METHOD4(axis, void(
        struct wl_pointer *wl_pointer,
        uint32_t time,
        uint32_t axis,
        wl_fixed_t value));

    MOCK_METHOD1(frame, void(struct wl_pointer *wl_pointer));

    MOCK_METHOD2(axis_source, void(struct wl_pointer *wl_pointer,
                        uint32_t axis_source));

    MOCK_METHOD3(axis_stop, void(struct wl_pointer *wl_pointer,
                      uint32_t time,
                      uint32_t axis));

    MOCK_METHOD3(axis_discrete, void(struct wl_pointer *wl_pointer,
                          uint32_t axis,
                          int32_t discrete));

    constexpr static wl_pointer_listener const thunks = {
        /*.enter = */       [](void* data, auto... args) { return static_cast<PointerListener*>(data)->enter(args...); },
        /*.leave = */       [](void* data, auto... args) { return static_cast<PointerListener*>(data)->leave(args...); },
        /*.motion = */      [](void* data, auto... args) { return static_cast<PointerListener*>(data)->motion(args...); },
        /*.button = */      [](void* data, auto... args) { return static_cast<PointerListener*>(data)->button(args...); },
        /*.axis = */        [](void* data, auto... args) { return static_cast<PointerListener*>(data)->axis(args...); },
        /*.frame = */       [](void* data, auto... args) { return static_cast<PointerListener*>(data)->frame(args...); },
        /*.axis_source = */ [](void* data, auto... args) { return static_cast<PointerListener*>(data)->axis_source(args...); },
        /*.axis_stop = */   [](void* data, auto... args) { return static_cast<PointerListener*>(data)->axis_stop(args...); },
        /*.axis_discrete = */[](void* data, auto... args) { return static_cast<PointerListener*>(data)->axis_discrete(args...); }
    };
};

constexpr wl_pointer_listener const PointerListener::thunks;
}

TEST_F(RelativePointer, default_pointer_gets_movement)
{
    auto const move_x = any_width/6;
    auto const move_y = any_height/6;

    PointerListener pointer_listener(a_client.the_pointer());

    EXPECT_CALL(pointer, relative_motion(_, _,
                                         wl_fixed_from_int(move_x), wl_fixed_from_int(move_y),
                                         wl_fixed_from_int(move_x), wl_fixed_from_int(move_y))).Times(1);

    EXPECT_CALL(pointer_listener, motion(_, _, _, _)).Times(1);

    cursor.move_by(move_x, move_y);
    a_client.roundtrip();
    a_client.roundtrip();
}
