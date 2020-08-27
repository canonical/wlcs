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

#include "pointer_constraints_unstable_v1.h"
#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{
auto const any_width = 300;
auto const any_height = 300;
auto const middle_x = any_width / 2;
auto const middle_y = any_height / 2;

struct PointerConstraints : StartedInProcessServer
{
    // Create a client with a surface and a cursor centered on the surface
    Client a_client{the_server()};
    Surface surface{a_client.create_visible_surface(any_width, any_height)};
    Pointer cursor = the_server().create_pointer();

    ZwpPointerConstraintsV1 pointer_constraints{a_client};

    void SetUp() override
    {
        StartedInProcessServer::SetUp();

        // Get the surface in a known position with the cursor over it
        the_server().move_surface_to(surface, 0, 0);
        cursor.move_to(middle_x, middle_y);
    }

    void TearDown() override
    {
        a_client.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(PointerConstraints, can_get_locked_pointer)
{
    ZwpLockedPointerV1 pointer{pointer_constraints, surface, a_client.the_pointer(), nullptr, 0};
    EXPECT_THAT(pointer, NotNull());
}

TEST_F(PointerConstraints, can_get_confined_pointer)
{
    ZwpConfinedPointerV1 pointer{pointer_constraints, surface, a_client.the_pointer(), nullptr, 0};
    EXPECT_THAT(pointer, NotNull());
}
