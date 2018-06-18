/*
 * Copyright © 2018 Canonical Ltd.
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
 * Authored by: William Wold <williamwold@canonical.com>
 */

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <vector>

using namespace testing;

using TouchTest = wlcs::InProcessServer;

TEST_F(TouchTest, touch_on_surface_seen)
{
    int const window_width = 300, window_height = 300;
    int const window_top_left_x = 64, window_top_left_y = 7;

    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(
        window_width,
        window_height);

    the_server().move_surface_to(surface, window_top_left_x, window_top_left_y);

    auto const wl_surface = static_cast<struct wl_surface*>(surface);

    auto touch = the_server().create_touch();
    int touch_x = window_top_left_x + 27;
    int touch_y = window_top_left_y + 8;

    touch.down_at(touch_x, touch_y);
    client.roundtrip();
    ASSERT_THAT(client.touched_window(), Eq(wl_surface)) << "touch did not register on surface";
    ASSERT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(touch_x - window_top_left_x),
                    wl_fixed_from_int(touch_y - window_top_left_y)))) << "touch came down in the wrong place";
    touch.up();
    client.roundtrip();
}
