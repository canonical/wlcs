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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <vector>
#include <memory>

using namespace testing;

struct TouchTestParams
{
    std::string name;
    std::function<std::unique_ptr<wlcs::Surface>(wlcs::InProcessServer& server, wlcs::Client& client,
                                                 int x, int y,
                                                 int width, int height)> make_surface;
};

std::ostream& operator<<(std::ostream& out, TouchTestParams const& param)
{
    return out << param.name;
}

class TouchTest:
    public wlcs::InProcessServer,
    public testing::WithParamInterface<TouchTestParams>
{
};

TEST_P(TouchTest, touch_on_surface_seen)
{
    int const window_width = 300, window_height = 300;
    int const window_top_left_x = 64, window_top_left_y = 7;

    wlcs::Client client{the_server()};
    auto const surface = GetParam().make_surface(*this, client,
                                                 window_top_left_x, window_top_left_y,
                                                 window_width, window_height);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

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

TEST_P(TouchTest, touch_and_drag_on_surface_seen)
{
    int const window_width = 300, window_height = 300;
    int const window_top_left_x = 64, window_top_left_y = 12;
    int const touch_x = window_top_left_x + 27, touch_y = window_top_left_y + 140;
    int const dx = 37, dy = -52;

    wlcs::Client client{the_server()};
    auto const surface = GetParam().make_surface(*this, client,
                                                 window_top_left_x, window_top_left_y,
                                                 window_width, window_height);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

    auto touch = the_server().create_touch();

    touch.down_at(touch_x, touch_y);
    client.roundtrip();
    ASSERT_THAT(client.touched_window(), Eq(wl_surface)) << "touch did not register on surface";
    ASSERT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(touch_x - window_top_left_x),
                    wl_fixed_from_int(touch_y - window_top_left_y)))) << "touch came down in the wrong place";

    touch.move_to(touch_x + dx, touch_y + dy);
    client.roundtrip();
    ASSERT_THAT(client.touched_window(), Eq(wl_surface)) << "surface was unfocused when it shouldn't have been";
    ASSERT_THAT(client.touch_position(),
                Ne(std::make_pair(
                    wl_fixed_from_int(touch_x - window_top_left_x),
                    wl_fixed_from_int(touch_y - window_top_left_y)))) << "touch did not move";
    ASSERT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(touch_x - window_top_left_x + dx),
                    wl_fixed_from_int(touch_y - window_top_left_y + dy)))) << "touch did not end up in the right place";

    touch.up();
    client.roundtrip();
}

TEST_P(TouchTest, touch_drag_outside_of_surface_and_back_not_lost)
{
    int const window_width = 300, window_height = 300;
    int const window_top_left_x = 64, window_top_left_y = 12;
    int const touch_a_x = window_top_left_x + 27, touch_a_y = window_top_left_y + 12;
    int const touch_b_x = window_top_left_x - 6, touch_b_y = window_top_left_y + window_width + 8;

    wlcs::Client client{the_server()};
    auto const surface = GetParam().make_surface(*this, client,
                                                 window_top_left_x, window_top_left_y,
                                                 window_width, window_height);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

    auto touch = the_server().create_touch();

    touch.down_at(touch_a_x, touch_a_y);
    client.roundtrip();
    ASSERT_THAT(client.touched_window(), Eq(wl_surface)) << "touch did not register on surface";
    ASSERT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(touch_a_x - window_top_left_x),
                    wl_fixed_from_int(touch_a_y - window_top_left_y)))) << "touch came down in the wrong place";

    touch.move_to(touch_b_x, touch_b_y);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Eq(wl_surface)) << "touch was lost when it moved out of the surface";
    if (client.touched_window() == wl_surface)
    {
        EXPECT_THAT(client.touch_position(),
                    Eq(std::make_pair(
                        wl_fixed_from_int(touch_b_x - window_top_left_x),
                        wl_fixed_from_int(touch_b_y - window_top_left_y)))) << "touch did not end up in the right place outside of the surface";
    }

    touch.move_to(touch_a_x, touch_a_y);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Eq(wl_surface)) << "touch did not come back onto surface";
    EXPECT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(touch_a_x - window_top_left_x),
                    wl_fixed_from_int(touch_a_y - window_top_left_y)))) << "touch came back in the wrong place";

    touch.up();
    client.roundtrip();
}

INSTANTIATE_TEST_CASE_P(
    WlShellSurface,
    TouchTest,
    testing::Values(
        TouchTestParams{
            "wl_shell_surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client, int x, int y, int width, int height)
                -> std::unique_ptr<wlcs::Surface>
                {
                    auto surface = client.create_wl_shell_surface(
                        width,
                        height);
                    server.the_server().move_surface_to(surface, x, y);
                    return std::make_unique<wlcs::Surface>(std::move(surface));
                }
            }
    ));

INSTANTIATE_TEST_CASE_P(
    XdgShellV6Surface,
    TouchTest,
    testing::Values(
        TouchTestParams{
            "xdg_v6_surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client, int x, int y, int width, int height)
                -> std::unique_ptr<wlcs::Surface>
                {
                    auto surface = client.create_xdg_shell_v6_surface(width, height);
                    server.the_server().move_surface_to(surface, x, y);
                    return std::make_unique<wlcs::Surface>(std::move(surface));;
                }
            }
    ));

INSTANTIATE_TEST_CASE_P(
    XdgShellStableSurface,
    TouchTest,
    testing::Values(
        TouchTestParams{
            "xdg_stable_surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client, int x, int y, int width, int height)
                -> std::unique_ptr<wlcs::Surface>
                {
                    auto surface = client.create_xdg_shell_stable_surface(width, height);
                    server.the_server().move_surface_to(surface, x, y);
                    return std::make_unique<wlcs::Surface>(std::move(surface));;
                }
            }
    ));

INSTANTIATE_TEST_CASE_P(
    Subsurface,
    TouchTest,
    testing::Values(
        TouchTestParams{
            "subsurface",
            [](wlcs::InProcessServer& server, wlcs::Client& client, int x, int y, int width, int height)
                -> std::unique_ptr<wlcs::Surface>
                {
                    auto main_surface = client.create_visible_surface(width, height);
                    server.the_server().move_surface_to(main_surface, x, y);
                    auto subusrface = wlcs::Subsurface::create_visible(main_surface, 0, 0, width, height);
                    client.run_on_destruction(
                        [main_surface = std::make_shared<wlcs::Surface>(std::move(main_surface))]() mutable
                        {
                            main_surface.reset();
                        });
                    return std::make_unique<wlcs::Surface>(std::move(subusrface));
                }
            }
    ));
