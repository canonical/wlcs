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
#include "xdg_shell_v6.h"

#include <gmock/gmock.h>

#include <vector>

using namespace testing;

class SubsurfaceTest : public wlcs::StartedInProcessServer
{
public:
    static int const surface_width = 200, surface_height = 300;
    static int const subsurface_width = 50, subsurface_height = 50;
    static int const surface_x = 20, surface_y = 30;

    SubsurfaceTest()
    {
        the_server().move_surface_to(main_surface, surface_x, surface_y);
        client.roundtrip();
    }

    void move_subsurface_to(int x, int y)
    {
        wl_subsurface_set_position(subsurface, x, y);
        wl_surface_commit(subsurface);
        wl_surface_commit(main_surface);
        client.roundtrip();
    }

    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(surface_width, surface_height)};
    wlcs::Subsurface subsurface{wlcs::Subsurface::create_visible(main_surface, 0, 0, subsurface_width, subsurface_height)};
};

TEST_F(SubsurfaceTest, subsurface_has_correct_parent)
{
    EXPECT_THAT(&subsurface.parent(), Eq(&main_surface));
}

TEST_F(SubsurfaceTest, subsurface_gets_pointer_input)
{
    int const pointer_x = surface_x + 10, pointer_y = surface_y + 5;

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - surface_x),
                    wl_fixed_from_int(pointer_y - surface_y))));
}

TEST_F(SubsurfaceTest, pointer_input_correctly_offset_for_subsurface)
{
    int const pointer_x = surface_x + 13, pointer_y = surface_y + 24;
    int const subsurface_x = 8, subsurface_y = 17;

    wl_subsurface_set_position(subsurface, subsurface_x, subsurface_y);
    wl_surface_commit(subsurface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - surface_x - subsurface_x),
                    wl_fixed_from_int(pointer_y - surface_y - subsurface_y))));
}

TEST_F(SubsurfaceTest, subsurface_extends_parent_input_region)
{
    int const pointer_x = surface_x - 5, pointer_y = surface_y + surface_height + 8;
    int const subsurface_x = -10, subsurface_y = surface_height - 10;

    move_subsurface_to(subsurface_x, subsurface_y);

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - surface_x - subsurface_x),
                    wl_fixed_from_int(pointer_y - surface_y - subsurface_y))));
}

TEST_F(SubsurfaceTest, input_falls_through_empty_subsurface_input_region)
{
    int const pointer_x = surface_x + 10, pointer_y = surface_y + 5;

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_surface_set_input_region(subsurface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(subsurface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)subsurface)) << "input was incorrectly caught by subsurface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)main_surface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - surface_x),
                    wl_fixed_from_int(pointer_y - surface_y))));
}

TEST_F(SubsurfaceTest, gets_input_over_surface_with_empty_region)
{
    int const pointer_x = surface_x + 32, pointer_y = surface_y + 21;

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_surface_set_input_region(main_surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - surface_x),
                    wl_fixed_from_int(pointer_y - surface_y))));
}

TEST_F(SubsurfaceTest, one_subsurface_to_another_fallthrough)
{
    int const pointer_x_0 = surface_x + 10, pointer_y_0 = surface_y + 3;
    int const pointer_x_1 = surface_x + 10, pointer_y_1 = surface_y + 10;
    int const pointer_x_2 = surface_x + 20, pointer_y_2 = surface_y + 10;
    int const subsurface_x = 0, subsurface_y = 5;
    move_subsurface_to(subsurface_x, subsurface_y);

    auto subsurface_top{wlcs::Subsurface::create_visible(main_surface, 0, 0, subsurface_width, subsurface_height)};

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_region_add(wl_region, 20, 0, 30, 50);
    wl_surface_set_input_region(subsurface_top, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(subsurface_top);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();

    pointer.move_to(pointer_x_0, pointer_y_0);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)main_surface)) << "main surface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_0 - surface_x),
                    wl_fixed_from_int(pointer_y_0 - surface_y))));

    pointer.move_to(pointer_x_1, pointer_y_1);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface)) << "lower subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_1 - surface_x - subsurface_x),
                    wl_fixed_from_int(pointer_y_1 - surface_y - subsurface_y))));

    pointer.move_to(pointer_x_2, pointer_y_2);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface_top)) << "upper subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_2 - surface_x),
                    wl_fixed_from_int(pointer_y_2 - surface_y))));
}

// TODO: subsurface of subsurface

// TODO: subsurface reordering (not in Mir yet)

// TODO: sync and desync
