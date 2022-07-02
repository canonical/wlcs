/*
 * Copyright © 2022 Canonical Ltd.
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

#include "in_process_server.h"
#include "version_specifier.h"
#include "xdg_output_v1.h"
#include "generated/wlr-virtual-pointer-unstable-v1-client.h"

#include "linux/input.h"
#include <gmock/gmock.h>

using namespace testing;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_virtual_pointer_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_virtual_pointer_v1)
}

namespace
{
class PointerListener
{
public:
    PointerListener(wl_seat* seat);
    ~PointerListener();

    MOCK_METHOD(void, enter, (uint32_t serial, wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y));
    MOCK_METHOD(void, leave, (uint32_t serial, wl_surface* surface));
    MOCK_METHOD(void, motion, (uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y));
    MOCK_METHOD(void, button, (uint32_t serial, uint32_t time, uint32_t button, uint32_t state));
    MOCK_METHOD(void, axis, (uint32_t time, uint32_t axis, wl_fixed_t value));
    MOCK_METHOD(void, frame, ());
    MOCK_METHOD(void, axis_source, (uint32_t axis_source));
    MOCK_METHOD(void, axis_stop, (uint32_t time, uint32_t axis));
    MOCK_METHOD(void, axis_discrete, (uint32_t axis, int32_t discrete));

private:
    wl_pointer* const proxy;
};

PointerListener::PointerListener(wl_seat* seat)
    : proxy{wl_seat_get_pointer(seat)}
{
#define FORWARD_TO_MOCK(method) \
    [](void* data, wl_pointer*, auto... args){ static_cast<PointerListener*>(data)->method(args...); }
    static const wl_pointer_listener listener = {
        FORWARD_TO_MOCK(enter),
        FORWARD_TO_MOCK(leave),
        FORWARD_TO_MOCK(motion),
        FORWARD_TO_MOCK(button),
        FORWARD_TO_MOCK(axis),
        FORWARD_TO_MOCK(frame),
        FORWARD_TO_MOCK(axis_source),
        FORWARD_TO_MOCK(axis_stop),
        FORWARD_TO_MOCK(axis_discrete),
    };
#undef FORWARD_TO_MOCK
    wl_pointer_add_listener(proxy, &listener, this);
}

PointerListener::~PointerListener()
{
    wl_pointer_destroy(proxy);
}

class VirtualPointerV1Test: public wlcs::StartedInProcessServer
{
public:
    VirtualPointerV1Test()
        : receive_client{the_server()},
          send_client{the_server()},
          surface{receive_client.create_visible_surface(surface_width, surface_height)},
          pointer{the_server().create_pointer()},
          listener{receive_client.seat()},
          manager{send_client.bind_if_supported<zwlr_virtual_pointer_manager_v1>(wlcs::AnyVersion)}
    {
        EXPECT_CALL(listener, enter(_, surface.operator wl_surface*(), _, _));
        EXPECT_CALL(listener, motion(_, _, _)).Times(AnyNumber());
        EXPECT_CALL(listener, frame()).Times(AnyNumber());
        the_server().move_surface_to(surface, 0, 0);
        pointer.move_to(pointer_start_x, pointer_start_y);
        send_client.roundtrip();
        receive_client.roundtrip();
        Mock::VerifyAndClearExpectations(&listener);
    }

    wlcs::Client receive_client;
    wlcs::Client send_client;
    wlcs::Surface surface;
    wlcs::Pointer pointer;
    StrictMock<PointerListener> listener;
    wlcs::WlHandle<zwlr_virtual_pointer_manager_v1> manager;

    static int const surface_width = 400;
    static int const surface_height = 400;
    static int const pointer_start_x = 20;
    static int const pointer_start_y = 30;
};
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_is_moved_client_sees_motion)
{
    int const motion_x = 7;
    int const motion_y = 22;

    EXPECT_CALL(listener, motion(_,
        wl_fixed_from_int(pointer_start_x + motion_x),
        wl_fixed_from_int(pointer_start_y + motion_y)));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));

    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_motion(handle, 0, wl_fixed_from_int(motion_x), wl_fixed_from_int(motion_y));
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_is_moved_multiple_times_client_sees_motion)
{
    int const motion1_x = 7;
    int const motion1_y = 22;
    int const motion2_x = 5;
    int const motion2_y = -12;

    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);

    EXPECT_CALL(listener, motion(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(listener, frame()).Times(AnyNumber());
    zwlr_virtual_pointer_v1_motion(handle, 0, wl_fixed_from_int(motion1_x), wl_fixed_from_int(motion1_y));
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();

    EXPECT_CALL(listener, motion(_,
        wl_fixed_from_int(pointer_start_x + motion1_x + motion2_x),
        wl_fixed_from_int(pointer_start_y + motion1_y + motion2_y)));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));
    zwlr_virtual_pointer_v1_motion(handle, 0, wl_fixed_from_int(motion2_x), wl_fixed_from_int(motion2_y));
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_clicks_client_sees_button)
{
    EXPECT_CALL(listener, button(_, _, BTN_LEFT, true));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_button(handle, 0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_scrolls_client_sees_axis)
{
    EXPECT_CALL(listener, axis(_, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(5)));
    EXPECT_CALL(listener, axis_source(_)).Times(AnyNumber());
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_axis(handle, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(5));
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_scrolls_with_steps_client_sees_axis_descrete)
{
    EXPECT_CALL(listener, axis(_, WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_int(5)));
    EXPECT_CALL(listener, axis_discrete(WL_POINTER_AXIS_HORIZONTAL_SCROLL, 4));
    EXPECT_CALL(listener, axis_source(_)).Times(AnyNumber());
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_axis_discrete(handle, 0, WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_int(5), 4);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_specifies_axis_source_client_sees_axis_source)
{
    EXPECT_CALL(listener, axis(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(listener, axis_source(WL_POINTER_AXIS_SOURCE_CONTINUOUS));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_axis(handle, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(5));
    zwlr_virtual_pointer_v1_axis_source(handle, WL_POINTER_AXIS_SOURCE_CONTINUOUS);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
    EXPECT_CALL(listener, axis_source(WL_POINTER_AXIS_SOURCE_WHEEL));
    zwlr_virtual_pointer_v1_axis(handle, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(5));
    zwlr_virtual_pointer_v1_axis_source(handle, WL_POINTER_AXIS_SOURCE_WHEEL);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, if_frame_is_not_sent_client_sees_no_events)
{
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_motion(handle, 0, wl_fixed_from_int(6), wl_fixed_from_int(7));
    zwlr_virtual_pointer_v1_motion_absolute(handle, 0, 2, 4, 10, 10);
    zwlr_virtual_pointer_v1_button(handle, 0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_axis(handle, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(5));
    zwlr_virtual_pointer_v1_axis_source(handle, WL_POINTER_AXIS_SOURCE_WHEEL);
    // Should produce no events because there has been no frame
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_is_moved_with_absolute_coordinates_with_the_extent_of_the_output_client_sees_motion)
{
    ASSERT_THAT(send_client.output_count(), Ge(1u));
    wlcs::XdgOutputManagerV1 xdg_output_manager{send_client};
    wlcs::XdgOutputV1 xdg_output{xdg_output_manager, 0};
    send_client.roundtrip();
    auto const& output_state = xdg_output.state();
    ASSERT_THAT(output_state.logical_size.operator bool(), Eq(true));
    auto const output_size = output_state.logical_size.value();

    int const move_to_x = 22;
    int const move_to_y = 33;

    EXPECT_CALL(listener, motion(_,
        wl_fixed_from_int(move_to_x),
        wl_fixed_from_int(move_to_y)));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));

    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_motion_absolute(handle, 0, move_to_x, move_to_y, output_size.first, output_size.second);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_is_moved_with_absolute_coordinates_with_the_extent_twice_of_the_output_client_sees_motion)
{
    ASSERT_THAT(send_client.output_count(), Ge(1u));
    wlcs::XdgOutputManagerV1 xdg_output_manager{send_client};
    wlcs::XdgOutputV1 xdg_output{xdg_output_manager, 0};
    send_client.roundtrip();
    auto const& output_state = xdg_output.state();
    ASSERT_THAT(output_state.logical_size.operator bool(), Eq(true));
    auto const output_size = output_state.logical_size.value();

    int const move_to_x = 22;
    int const move_to_y = 33;

    EXPECT_CALL(listener, motion(_,
        wl_fixed_from_int(move_to_x),
        wl_fixed_from_int(move_to_y)));
    EXPECT_CALL(listener, frame()).Times(AtLeast(1));

    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    zwlr_virtual_pointer_v1_motion_absolute(
        handle, 0,
        move_to_x * 2, move_to_y * 2,
        output_size.first * 2, output_size.second * 2);
    zwlr_virtual_pointer_v1_frame(handle);
    send_client.roundtrip();
    receive_client.roundtrip();
}
