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

#include <gmock/gmock.h>
#include <linux/input-event-codes.h>

using namespace testing;

namespace
{
struct KeyboardTest: public wlcs::StartedInProcessServer
{
    int const window_width = 300, window_height = 300;
    int const surface1_x = 100, surface1_y = 100;

    wlcs::Client client1{the_server()};
    wlcs::Pointer pointer{the_server().create_pointer()};

    wlcs::Keyboard keyboard;

    wlcs::Surface surface1;
    struct wl_surface* wl_surface1;

    KeyboardTest() :
        keyboard{the_server().create_keyboard()},
        surface1{client1.create_visible_surface(window_width, window_height)},
        wl_surface1{static_cast<struct wl_surface*>(surface1)}
    {
        the_server().move_surface_to(surface1, surface1_x, surface1_y);
    }
};
}

TEST_F(KeyboardTest, key_press_on_focused_surface_seen)
{
    pointer.move_to(surface1_x + 50, surface1_y + 50);
    pointer.left_click();
    client1.roundtrip();

    ASSERT_THAT(client1.keyboard_focused_window(), Eq(wl_surface1))
        << "Surface should have keyboard focus after pointer entered";

    keyboard.key_down(KEY_A);
    client1.roundtrip();

    auto key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue()) << "Should have received a key event";
    EXPECT_THAT(key_event->scancode, Eq(KEY_A)) << "Key code should be KEY_A";
    EXPECT_THAT(key_event->is_down, IsTrue()) << "Event should be a key down";

    keyboard.key_up(KEY_A);
    client1.roundtrip();

    key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue()) << "Should have received a key event";
    EXPECT_THAT(key_event->scancode, Eq(KEY_A)) << "Key code should be KEY_A";
    EXPECT_THAT(key_event->is_down, IsFalse()) << "Event should be a key up";
}

TEST_F(KeyboardTest, key_press_without_focus_not_seen)
{
    int const surface2_x = 500, surface2_y = 100;

    wlcs::Client client2{the_server()};
    auto surface2 = client2.create_visible_surface(window_width, window_height);
    the_server().move_surface_to(surface2, surface2_x, surface2_y);
    auto const wl_surface2 = static_cast<struct wl_surface*>(surface2);

    // Move pointer to surface1 to give it focus
    pointer.move_to(surface1_x + 50, surface1_y + 50);
    pointer.left_click();
    client1.roundtrip();
    client2.roundtrip();

    ASSERT_THAT(client1.keyboard_focused_window(), Eq(wl_surface1))
        << "Surface1 should have keyboard focus";
    EXPECT_THAT(client2.keyboard_focused_window(), Ne(wl_surface2))
        << "Surface2 should NOT have keyboard focus";

    keyboard.key_down(KEY_B);
    client1.roundtrip();
    client2.roundtrip();

    auto key_event1 = client1.last_key_event();
    ASSERT_THAT(key_event1.has_value(), IsTrue())
        << "Focused surface should have received a key event";
    EXPECT_THAT(key_event1->scancode, Eq(KEY_B)) << "Key code should be KEY_B";
    EXPECT_THAT(key_event1->is_down, IsTrue()) << "Event should be a key down";

    auto key_event2 = client2.last_key_event();
    EXPECT_THAT(key_event2.has_value(), IsFalse())
        << "Unfocused surface should NOT have received a key event";
}

TEST_F(KeyboardTest, modifier_keys_basic)
{
    pointer.move_to(surface1_x + 50, surface1_y + 50);
    pointer.left_click();
    client1.roundtrip();

    ASSERT_THAT(client1.keyboard_focused_window(), Eq(wl_surface1))
        << "Surface should have keyboard focus";

    keyboard.key_down(KEY_LEFTSHIFT);
    client1.roundtrip();

    auto key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue());
    EXPECT_THAT(key_event->scancode, Eq(KEY_LEFTSHIFT));
    EXPECT_THAT(key_event->is_down, IsTrue());

    keyboard.key_down(KEY_A);
    client1.roundtrip();

    key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue());
    EXPECT_THAT(key_event->scancode, Eq(KEY_A));
    EXPECT_THAT(key_event->is_down, IsTrue());

    keyboard.key_up(KEY_A);
    client1.roundtrip();

    key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue());
    EXPECT_THAT(key_event->scancode, Eq(KEY_A));
    EXPECT_THAT(key_event->is_down, IsFalse());

    keyboard.key_up(KEY_LEFTSHIFT);
    client1.roundtrip();

    key_event = client1.last_key_event();
    ASSERT_THAT(key_event.has_value(), IsTrue());
    EXPECT_THAT(key_event->scancode, Eq(KEY_LEFTSHIFT));
    EXPECT_THAT(key_event->is_down, IsFalse());
}
