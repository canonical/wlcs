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
