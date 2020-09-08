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
auto const nw_middle_x = any_width / 2;
auto const nw_middle_y = any_height / 2;
auto const se_middle_x = any_width + nw_middle_x;
auto const se_middle_y = any_height + nw_middle_y;

struct PointerConstraints : StartedInProcessServer
{
    // Create a client with two surface (ne & sw) and a cursor centered on the nw_surface
    Client client{the_server()};
    Surface se_surface{client.create_visible_surface(any_width, any_height)};
    Surface nw_surface{client.create_visible_surface(any_width, any_height)};
    wl_pointer* const pointer = client.the_pointer();

    Pointer cursor = the_server().create_pointer();

    ZwpPointerConstraintsV1 pointer_constraints{client};

    void SetUp() override
    {
        StartedInProcessServer::SetUp();

        // Get the surface in a known position with the cursor over it
        the_server().move_surface_to(nw_surface, 0, 0);
        cursor.move_to(nw_middle_x, nw_middle_y);
        the_server().move_surface_to(se_surface, any_width, any_height);
    }

    void TearDown() override
    {
        client.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(PointerConstraints, can_get_locked_pointer)
{
    ZwpLockedPointerV1 locked_ptr{pointer_constraints, nw_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};
    EXPECT_THAT(locked_ptr, NotNull());
}

TEST_F(PointerConstraints, locked_pointer_on_initially_focussed_surface_gets_locked_notification)
{
    ZwpLockedPointerV1 locked_ptr{pointer_constraints, nw_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};

    EXPECT_CALL(locked_ptr, locked()).Times(1);

    client.roundtrip();
}

TEST_F(PointerConstraints, locked_pointer_does_not_move)
{
    auto const initial_cursor_position = client.pointer_position();
    ZwpLockedPointerV1 locked_ptr{pointer_constraints, nw_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};
    EXPECT_CALL(locked_ptr, locked()).Times(AnyNumber());
    client.roundtrip();

    cursor.move_by(10, 10);
    client.roundtrip();

    EXPECT_THAT(client.pointer_position(), Eq(initial_cursor_position));
}

TEST_F(PointerConstraints, locked_pointer_on_initially_unfocussed_surface_gets_no_locked_notification)
{
    ZwpLockedPointerV1 locked_ptr{pointer_constraints, se_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};

    EXPECT_CALL(locked_ptr, locked()).Times(0);

    client.roundtrip();
}

TEST_F(PointerConstraints, when_cursor_clicks_on_surface_locked_pointer_gets_locked_notification)
{
    ZwpLockedPointerV1 locked_ptr{pointer_constraints, se_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};

    EXPECT_CALL(locked_ptr, locked()).Times(1);

    cursor.move_to(se_middle_x, se_middle_y);
    cursor.left_click();
    client.roundtrip();
}

TEST_F(PointerConstraints, can_get_confined_pointer)
{
    ZwpConfinedPointerV1 confined_ptr{pointer_constraints, nw_surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT};
    EXPECT_THAT(confined_ptr, NotNull());
}
