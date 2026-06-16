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

struct DataDeviceDragAndDropTest : StartedInProcessServer
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

    // The surface the drag is currently over, tracked via the data device's
    // enter/leave events (leave itself carries no surface, so we have to
    // remember which surface was last entered).
    wl_surface* surface_under_pointer = nullptr;

    DataDeviceDragAndDropTest()
    {
        ON_CALL(device_listener, enter(_, _, _, _, _, _))
            .WillByDefault(Invoke(
                [this](wl_data_device*, uint32_t, wl_surface* surface, wl_fixed_t, wl_fixed_t, wl_data_offer*)
                {
                    surface_under_pointer = surface;
                }));
        ON_CALL(device_listener, leave(_))
            .WillByDefault(Invoke([this](wl_data_device*) { surface_under_pointer = nullptr; }));

        the_server().move_surface_to(source_surface, source_x, source_y);
        the_server().move_surface_to(target_surface, target_x, target_y);
        client.roundtrip();
    }

    void TearDown() override
    {
        client.roundtrip();
        StartedInProcessServer::TearDown();
    }

    // Move the pointer to the centre of the source surface.
    void move_pointer_to_source()
    {
        pointer.move_to(source_x + surface_width / 2, source_y + surface_height / 2);
    }

    // Press the left button, returning the serial of the button-press event
    // (which is needed to start a drag).
    auto press_pointer() -> uint32_t
    {
        uint32_t button_serial{0};
        bool button_down{false};
        client.add_pointer_button_notification(
            [&](uint32_t serial, uint32_t, bool)
            {
                button_serial = serial;
                button_down = true;
                return false;
            });
        pointer.left_button_down();
        client.dispatch_until([&]() { return button_down; });

        return button_serial;
    }

    // Begin a drag from the source surface, offering mime_type, using the grab
    // identified by button_serial.
    void start_drag(uint32_t button_serial, char const* mime_type)
    {
        wl_data_source_offer(data_source, mime_type);
        wl_data_device_start_drag(
            data_device, data_source, source_surface, nullptr, button_serial);
        client.roundtrip();
    }

    // Move the pointer over the target surface and wait until the drag has
    // entered it. Waiting on the tracked surface (rather than just any enter
    // event) also drains the leave of the source surface, so that a subsequent
    // wait in the test isn't satisfied by a stale event.
    void move_pointer_to_target()
    {
        pointer.move_to(target_x + surface_width / 2, target_y + surface_height / 2);
        client.dispatch_until([this]() { return surface_under_pointer == target_surface.wl_surface(); });
    }
};
}

TEST_F(DataDeviceDragAndDropTest, data_device_receives_offer_when_drag_enters_target_surface)
{
    EXPECT_CALL(device_listener, data_offer(_, _)).Times(AtLeast(1));

    move_pointer_to_source();
    auto const button_serial = press_pointer();
    start_drag(button_serial, any_mime_type);
    move_pointer_to_target();
}

TEST_F(DataDeviceDragAndDropTest, data_offer_advertises_source_mime_type)
{
    // A distinctive mime type, so we can be sure the advertised type is the one
    // the source offered rather than something the compositor added itself.
    char const* const specific_mime_type = "application/x-wlcs-test-mime";

    MockDataOfferListener offer_listener;

    EXPECT_CALL(device_listener, data_offer(_, _))
        .WillRepeatedly(Invoke(
            [&](wl_data_device*, wl_data_offer* offer) { offer_listener.listen_to(offer); }));

    bool got_mime_type{false};
    EXPECT_CALL(offer_listener, offer(_, StrEq(specific_mime_type)))
        .WillRepeatedly(Invoke([&](wl_data_offer*, char const*) { got_mime_type = true; }));

    move_pointer_to_source();
    auto const button_serial = press_pointer();
    start_drag(button_serial, specific_mime_type);
    client.dispatch_until([&]() { return got_mime_type; });
}

TEST_F(DataDeviceDragAndDropTest, data_device_receives_motion_during_drag)
{
    bool got_motion{false};
    EXPECT_CALL(device_listener, motion(_, _, _, _))
        .WillRepeatedly(Invoke([&](wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) { got_motion = true; }));

    move_pointer_to_source();
    auto const button_serial = press_pointer();
    start_drag(button_serial, any_mime_type);
    move_pointer_to_target();

    got_motion = false;
    pointer.move_to(target_x + surface_width / 4, target_y + surface_height / 4);
    client.dispatch_until([&]() { return got_motion; });
}

TEST_F(DataDeviceDragAndDropTest, data_device_receives_leave_when_drag_leaves_target_surface)
{
    move_pointer_to_source();
    auto const button_serial = press_pointer();
    start_drag(button_serial, any_mime_type);
    move_pointer_to_target();

    // Moving off the target should leave it. We check the tracked surface, so a
    // leave of any other surface (e.g. the source) can't satisfy this.
    pointer.move_to(target_x + surface_width + 50, target_y + surface_height + 50);
    client.dispatch_until([this]() { return surface_under_pointer != target_surface.wl_surface(); });
}

TEST_F(DataDeviceDragAndDropTest, data_device_receives_drop_when_button_released_over_target_surface)
{
    bool dropped{false};
    EXPECT_CALL(device_listener, drop(_))
        .WillRepeatedly(Invoke([&](wl_data_device*) { dropped = true; }));

    move_pointer_to_source();
    auto const button_serial = press_pointer();
    start_drag(button_serial, any_mime_type);
    move_pointer_to_target();

    pointer.left_button_up();
    client.dispatch_until([&]() { return dropped; });
}
