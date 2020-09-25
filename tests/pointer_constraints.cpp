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

#include <memory>

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

    std::unique_ptr<ZwpLockedPointerV1> locked_ptr = nullptr;
    std::unique_ptr<ZwpConfinedPointerV1> confined_ptr = nullptr;

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
        locked_ptr.reset();
        confined_ptr.reset();
        client.roundtrip();
        StartedInProcessServer::TearDown();
    }

    void setup_locked_ptr_on(Surface& surface, zwp_pointer_constraints_v1_lifetime lifetime = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT)
    {
        locked_ptr = std::make_unique<ZwpLockedPointerV1>(pointer_constraints, surface, pointer, nullptr, lifetime);
    }

    void setup_confined_ptr_on(Surface& surface, zwp_pointer_constraints_v1_lifetime lifetime = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT)
    {
        confined_ptr = std::make_unique<ZwpConfinedPointerV1>(pointer_constraints, surface, pointer, nullptr, lifetime);
    }

    void setup_sync()
    {
        client.roundtrip();
        using namespace testing;
        if (locked_ptr) Mock::VerifyAndClearExpectations(locked_ptr.get());
        if (confined_ptr) Mock::VerifyAndClearExpectations(confined_ptr.get());
    }

    void select_se_window()
    {
        cursor.move_to(se_middle_x, se_middle_y);
        cursor.left_click();
        client.roundtrip();
    }

    void select_nw_window()
    {
        cursor.move_to(nw_middle_x, nw_middle_y);
        cursor.left_click();
        client.roundtrip();
    }
};
}

TEST_F(PointerConstraints, can_get_locked_pointer)
{
    setup_locked_ptr_on(nw_surface);

    EXPECT_THAT(*locked_ptr, NotNull());
}

TEST_F(PointerConstraints, locked_pointer_on_initially_focussed_surface_gets_locked_notification)
{
    setup_locked_ptr_on(nw_surface);

    EXPECT_CALL(*locked_ptr, locked()).Times(1);

    client.roundtrip();
}

TEST_F(PointerConstraints, locked_pointer_does_not_move)
{
    auto const initial_pointer_position = client.pointer_position();
    setup_locked_ptr_on(nw_surface);
    EXPECT_CALL(*locked_ptr, locked()).Times(AnyNumber());
    setup_sync();

    cursor.move_by(10, 10);
    client.roundtrip();

    EXPECT_THAT(client.pointer_position(), Eq(initial_pointer_position));
}

TEST_F(PointerConstraints, locked_pointer_on_initially_unfocussed_surface_gets_no_locked_notification)
{
    setup_locked_ptr_on(se_surface);

    EXPECT_CALL(*locked_ptr, locked()).Times(0);

    client.roundtrip();
}

TEST_F(PointerConstraints, when_surface_is_selected_locked_pointer_gets_locked_notification)
{
    setup_locked_ptr_on(se_surface);
    setup_sync();

    EXPECT_CALL(*locked_ptr, locked()).Times(1);

    select_se_window();
}

TEST_F(PointerConstraints, when_surface_is_unselected_locked_pointer_gets_unlocked_notification)
{
    setup_locked_ptr_on(se_surface);
    EXPECT_CALL(*locked_ptr, locked()).Times(AnyNumber());
    setup_sync();

    EXPECT_CALL(*locked_ptr, unlocked()).Times(1);

    select_nw_window();
}

TEST_F(PointerConstraints, can_get_confined_pointer)
{
    setup_confined_ptr_on(nw_surface);
    EXPECT_THAT(*confined_ptr, NotNull());
}
