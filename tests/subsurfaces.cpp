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

struct SubsurfaceTestParams
{
    std::string name;
    std::function<std::unique_ptr<wlcs::Surface>(wlcs::InProcessServer& server,
                                                 wlcs::Client& client,
                                                 int x, int y,
                                                 int width, int height)> make_surface;
};

std::ostream& operator<<(std::ostream& out, SubsurfaceTestParams const& param)
{
    return out << param.name;
}

class SubsurfaceTest :
    public wlcs::StartedInProcessServer,
    public testing::WithParamInterface<SubsurfaceTestParams>
{
public:
    static int const surface_width = 200, surface_height = 300;
    static int const subsurface_width = 50, subsurface_height = 50;
    static int const surface_x = 20, surface_y = 30;

    SubsurfaceTest():
        client{the_server()},
        main_surface{std::move(*GetParam().make_surface(
            *this, client, surface_x, surface_y, surface_width, surface_height))},
        subsurface{wlcs::Subsurface::create_visible(main_surface, 0, 0, subsurface_width, subsurface_height)}

    {
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
    wlcs::Surface main_surface;
    wlcs::Subsurface subsurface;
};

TEST_P(SubsurfaceTest, subsurface_has_correct_parent)
{
    EXPECT_THAT(&subsurface.parent(), Eq(&main_surface));
}

TEST_P(SubsurfaceTest, subsurface_gets_pointer_input)
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

TEST_P(SubsurfaceTest, pointer_input_correctly_offset_for_subsurface)
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

TEST_P(SubsurfaceTest, sync_mode_works_correctly)
{
    int const pointer_x = 30, pointer_y = 30;
    int const subsurface_x_0 = 10, subsurface_y_0 = 10;
    int const subsurface_x_1 = 20, subsurface_y_1 = 20;

    wl_subsurface_set_position(subsurface, subsurface_x_0, subsurface_y_0);
    wl_subsurface_set_sync(subsurface);
    wl_surface_commit(subsurface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))));

    wl_subsurface_set_position(subsurface, subsurface_x_1, subsurface_y_1);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface moved when position was set, before either surface was committed";

    wl_surface_commit(subsurface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface moved when committed, before parent committed";

    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface not in the right place after it should have moved";

    wl_subsurface_set_position(subsurface, subsurface_x_0, subsurface_y_0);
    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface moved when parent committed, but before it committed";

    wl_surface_commit(subsurface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface moved when parent committed, then it committed in that order, rather then reverse";

    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface not in the right place after it should have moved a second time";
}

TEST_P(SubsurfaceTest, desync_mode_works_correctly)
{
    int const pointer_x = 30, pointer_y = 30;
    int const subsurface_x_0 = 10, subsurface_y_0 = 10;
    int const subsurface_x_1 = 20, subsurface_y_1 = 20;

    wl_subsurface_set_position(subsurface, subsurface_x_0, subsurface_y_0);
    wl_subsurface_set_desync(subsurface);
    wl_surface_commit(subsurface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface));
    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))));

    wl_subsurface_set_position(subsurface, subsurface_x_1, subsurface_y_1);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface moved when position was set, before either surface was committed";

    wl_surface_commit(subsurface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface not when committed, before parent committed";

    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface not in the right place after it should have moved";

    wl_subsurface_set_position(subsurface, subsurface_x_0, subsurface_y_0);
    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_1),
                    wl_fixed_from_int(pointer_y - subsurface_y_1))))
        << "subsurface moved when parent committed, but before it committed";

    wl_surface_commit(subsurface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface not moved when parent committed, then it committed in that order, rather then reverse";

    wl_surface_commit(main_surface);
    client.roundtrip();

    pointer.move_to(0, 0);
    pointer.move_to(pointer_x + surface_x, pointer_y + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - subsurface_x_0),
                    wl_fixed_from_int(pointer_y - subsurface_y_0))))
        << "subsurface not in the right place after it should have moved a second time";
}

TEST_P(SubsurfaceTest, subsurface_extends_parent_input_region)
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

TEST_P(SubsurfaceTest, input_falls_through_empty_subsurface_input_region)
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

TEST_P(SubsurfaceTest, gets_input_over_surface_with_empty_region)
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

TEST_P(SubsurfaceTest, one_subsurface_to_another_fallthrough)
{
    int const pointer_x_0 = 3, pointer_y_0 = 3;
    int const pointer_x_1 = 3, pointer_y_1 = 10;
    int const pointer_x_2 = 10, pointer_y_2 = 3;
    int const subsurface_x = 0, subsurface_y = 5;
    int const subsurface_top_x = 5, subsurface_top_y = 0;
    move_subsurface_to(subsurface_x, subsurface_y);
    auto subsurface_top{wlcs::Subsurface::create_visible(main_surface, subsurface_top_x, subsurface_top_y, subsurface_width, subsurface_height)};

    auto pointer = the_server().create_pointer();

    pointer.move_to(pointer_x_0 + surface_x, pointer_y_0 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)main_surface)) << "main surface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_0),
                    wl_fixed_from_int(pointer_y_0))));

    pointer.move_to(pointer_x_1 + surface_x, pointer_y_1 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface)) << "lower subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_1 - subsurface_x),
                    wl_fixed_from_int(pointer_y_1 - subsurface_y))));

    pointer.move_to(pointer_x_2 + surface_x, pointer_y_2 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface_top)) << "upper subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_2 - subsurface_top_x),
                    wl_fixed_from_int(pointer_y_2 - subsurface_top_y))));
}

TEST_P(SubsurfaceTest, subsurface_of_a_subsurface_handled)
{
    int const pointer_x_0 = 3, pointer_y_0 = 3;
    int const pointer_x_1 = 3, pointer_y_1 = 10;
    int const pointer_x_2 = 10, pointer_y_2 = 3;
    int const subsurface_x = 0, subsurface_y = 5;
    int const subsurface_top_x = 5, subsurface_top_y = -5;
    move_subsurface_to(subsurface_x, subsurface_y);
    auto subsurface_top{wlcs::Subsurface::create_visible(subsurface, subsurface_top_x, subsurface_top_y, subsurface_width, subsurface_height)};

    auto pointer = the_server().create_pointer();

    pointer.move_to(pointer_x_0 + surface_x, pointer_y_0 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)main_surface)) << "main surface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_0),
                    wl_fixed_from_int(pointer_y_0))));

    pointer.move_to(pointer_x_1 + surface_x, pointer_y_1 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface)) << "lower subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_1 - subsurface_x),
                    wl_fixed_from_int(pointer_y_1 - subsurface_y))));

    pointer.move_to(pointer_x_2 + surface_x, pointer_y_2 + surface_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)subsurface_top)) << "subsurface of subsurface not focused";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x_2 - subsurface_top_x - subsurface_x),
                    wl_fixed_from_int(pointer_y_2 - subsurface_top_y - subsurface_y))));
}

INSTANTIATE_TEST_CASE_P(
    WlShellSubsurfaces,
    SubsurfaceTest,
    testing::Values(
        SubsurfaceTestParams{
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
    XdgShellSubsurfaces,
    SubsurfaceTest,
    testing::Values(
        SubsurfaceTestParams{
            "xdg_v6_surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client, int x, int y, int width, int height)
                -> std::unique_ptr<wlcs::Surface>
                {
                    auto surface = client.create_xdg_shell_v6_surface(
                        width,
                        height);
                    server.the_server().move_surface_to(surface, x, y);
                    return std::make_unique<wlcs::Surface>(std::move(surface));
                }
        }
    ));

// TODO: subsurface reordering (not in Mir yet)

// TODO: combinations of sync and desync at various levels of the tree
