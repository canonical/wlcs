/*
 * Copyright © 2026 Canonical Ltd.
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

#include "in_process_server.h"
#include "data_device.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

#include <memory>
#include <optional>

using namespace testing;
using namespace wlcs;

namespace
{
int constexpr surface_width = 100;
int constexpr surface_height = 100;

int constexpr source_x = 0;
int constexpr source_y = 0;
int constexpr target_x = 200;
int constexpr target_y = 200;

char const* const any_mime_type = "text/plain";

struct MockDataDeviceListener : DataDeviceListener
{
    using DataDeviceListener::DataDeviceListener;

    MOCK_METHOD(void, data_offer, (wl_data_device*, wl_data_offer*), (override));
    MOCK_METHOD(void, enter,
        (wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer*), (override));
    MOCK_METHOD(void, leave, (wl_data_device*), (override));
    MOCK_METHOD(void, motion, (wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t), (override));
    MOCK_METHOD(void, drop, (wl_data_device*), (override));
};

struct MockDataOfferListener : DataOfferListener
{
    using DataOfferListener::DataOfferListener;

    MOCK_METHOD(void, offer, (wl_data_offer*, char const*), (override));
};

struct DragAndDrop : StartedInProcessServer
{
    Client client{the_server()};

    WlHandle<wl_data_device_manager> const manager{
        client.bind_if_supported<wl_data_device_manager>(AnyVersion)};
    DataDevice data_device{wl_data_device_manager_get_data_device(manager, client.seat())};
    NiceMock<MockDataDeviceListener> device_listener{data_device};
    DataSource data_source{wl_data_device_manager_create_data_source(manager)};

    Surface source_surface{client.create_visible_surface(surface_width, surface_height)};
    Surface target_surface{client.create_visible_surface(surface_width, surface_height)};

    wlcs::Pointer pointer = the_server().create_pointer();

    DragAndDrop()
    {
        the_server().move_surface_to(source_surface, source_x, source_y);
        the_server().move_surface_to(target_surface, target_x, target_y);
        client.roundtrip();
    }

    void TearDown() override
    {
        client.roundtrip();
        StartedInProcessServer::TearDown();
    }

    // Press the pointer over the source surface and begin a drag, offering
    // any_mime_type. Returns once the compositor has processed the request.
    void start_drag()
    {
        pointer.move_to(source_x + surface_width / 2, source_y + surface_height / 2);

        std::optional<uint32_t> button_serial;
        client.add_pointer_button_notification(
            [&](uint32_t serial, uint32_t, bool is_down)
            {
                if (is_down)
                    button_serial = serial;
                return false;
            });
        pointer.left_button_down();
        client.dispatch_until([&]() { return button_serial.has_value(); });

        wl_data_source_offer(data_source, any_mime_type);
        wl_data_device_start_drag(
            data_device, data_source, source_surface, nullptr, button_serial.value());
        client.roundtrip();
    }

    // Move the pointer over the target surface and wait until the drag has
    // entered it.
    void drag_over_target()
    {
        auto const entered_target = std::make_shared<bool>(false);
        EXPECT_CALL(device_listener, enter(_, _, _, _, _, _))
            .WillRepeatedly(Invoke(
                [this, entered_target](wl_data_device*, uint32_t, wl_surface* surface, wl_fixed_t, wl_fixed_t, wl_data_offer*)
                {
                    if (surface == target_surface.wl_surface())
                        *entered_target = true;
                }));

        pointer.move_to(target_x + surface_width / 2, target_y + surface_height / 2);
        client.dispatch_until([entered_target]() { return *entered_target; });
    }
};
}

TEST_F(DragAndDrop, data_device_receives_offer_when_drag_enters_target_surface)
{
    EXPECT_CALL(device_listener, data_offer(_, _)).Times(AtLeast(1));

    start_drag();
    drag_over_target();
}

TEST_F(DragAndDrop, data_offer_advertises_source_mime_type)
{
    MockDataOfferListener offer_listener;

    EXPECT_CALL(device_listener, data_offer(_, _))
        .WillRepeatedly(Invoke(
            [&](wl_data_device*, wl_data_offer* offer) { offer_listener.listen_to(offer); }));

    bool got_mime_type{false};
    EXPECT_CALL(offer_listener, offer(_, StrEq(any_mime_type)))
        .WillRepeatedly(Invoke([&](wl_data_offer*, char const*) { got_mime_type = true; }));

    start_drag();
    client.dispatch_until([&]() { return got_mime_type; });
}

TEST_F(DragAndDrop, data_device_receives_motion_during_drag)
{
    bool got_motion{false};
    EXPECT_CALL(device_listener, motion(_, _, _, _))
        .WillRepeatedly(Invoke([&](wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) { got_motion = true; }));

    start_drag();
    drag_over_target();

    got_motion = false;
    pointer.move_to(target_x + surface_width / 4, target_y + surface_height / 4);
    client.dispatch_until([&]() { return got_motion; });
}

TEST_F(DragAndDrop, data_device_receives_leave_when_drag_leaves_target_surface)
{
    bool left{false};
    EXPECT_CALL(device_listener, leave(_))
        .WillRepeatedly(Invoke([&](wl_data_device*) { left = true; }));

    start_drag();
    drag_over_target();

    // Any leave events from moving off the source surface have already
    // happened; only interested in leaving the target now.
    left = false;
    pointer.move_to(target_x + surface_width + 50, target_y + surface_height + 50);
    client.dispatch_until([&]() { return left; });
}

TEST_F(DragAndDrop, data_device_receives_drop_when_button_released_over_target_surface)
{
    bool dropped{false};
    EXPECT_CALL(device_listener, drop(_))
        .WillRepeatedly(Invoke([&](wl_data_device*) { dropped = true; }));

    start_drag();
    drag_over_target();

    pointer.left_button_up();
    client.dispatch_until([&]() { return dropped; });
}
