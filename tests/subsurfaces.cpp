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

using SubsurfaceTest = wlcs::InProcessServer;

TEST_F(SubsurfaceTest, subsurface_can_be_created)
{
    wlcs::Client client{the_server()};
    ASSERT_THAT(client.subcompositor(), NotNull());
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};
    wlcs::Subsurface subsurface{main_surface};
    EXPECT_THAT(&subsurface.parent(), Eq(&main_surface));
}

TEST_F(SubsurfaceTest, subsurface_gets_pointer_input)
{
    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};

    the_server().move_surface_to(main_surface, 20, 30);
    client.roundtrip();

    auto subsurface{wlcs::Subsurface::create_visible(main_surface, 0, 0, 50, 50)};

    auto pointer = the_server().create_pointer();
    pointer.move_to(30, 35);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(10),
                    wl_fixed_from_int(5))));
}

TEST_F(SubsurfaceTest, subsurface_pointer_input_offset)
{
    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};

    the_server().move_surface_to(main_surface, 20, 30);
    client.roundtrip();

    auto subsurface{wlcs::Subsurface::create_visible(main_surface, 8, 17, 50, 50)};

    auto pointer = the_server().create_pointer();
    pointer.move_to(35, 60);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(7),
                    wl_fixed_from_int(13))));
}

TEST_F(SubsurfaceTest, subsurface_empty_input_region_fallthrough)
{
    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};

    the_server().move_surface_to(main_surface, 20, 30);
    client.roundtrip();

    auto subsurface{wlcs::Subsurface::create_visible(main_surface, 5, 5, 50, 50)};
    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_surface_set_input_region(subsurface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(subsurface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(30, 40);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)subsurface)) << "input was incorrectly caught by subsurface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)main_surface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(10),
                    wl_fixed_from_int(10))));
}

TEST_F(SubsurfaceTest, subsurface_gets_input_over_surface_with_empty_region)
{
    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};

    the_server().move_surface_to(main_surface, 20, 30);
    client.roundtrip();

    auto subsurface{wlcs::Subsurface::create_visible(main_surface, 8, 17, 50, 50)};

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_surface_set_input_region(main_surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(35, 60);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((wl_surface*)main_surface)) << "input fell through to main surface";
    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(7),
                    wl_fixed_from_int(13))));
}

// TODO: William will submit PR to Mir that fixes tested behavior. Enable when that lands.
TEST_F(SubsurfaceTest, DISABLED_one_subsurface_to_another_fallthrough)
{
    wlcs::Client client{the_server()};
    wlcs::Surface main_surface{client.create_visible_surface(200, 300)};

    the_server().move_surface_to(main_surface, 20, 30);
    client.roundtrip();

    auto subsurface_bottom{wlcs::Subsurface::create_visible(main_surface, 0, 5, 50, 50)};
    auto subsurface_top{wlcs::Subsurface::create_visible(main_surface, 0, 0, 50, 50)};

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_region_add(wl_region, 20, 0, 30, 50);
    wl_surface_set_input_region(subsurface_top, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(subsurface_top);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();

    pointer.move_to(30, 33);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)main_surface)) << "main surface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(10),
                    wl_fixed_from_int(3))));

    pointer.move_to(30, 40);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface_bottom)) << "lower subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(10),
                    wl_fixed_from_int(5))));

    pointer.move_to(50, 40);
    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq((wl_surface*)subsurface_top)) << "upper subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(30),
                    wl_fixed_from_int(10))));
}

// TODO: subsurface reordering

// TODO: sync and desync
