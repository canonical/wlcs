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
auto static const any_width = 100;
auto static const any_height = 100;

struct RelativePointer : StartedInProcessServer
{
    Client a_client{the_server()};
    Surface const a_surface{a_client.create_visible_surface(any_width, any_height)};

    void TearDown() override
    {
        a_client.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(RelativePointer, can_get_relative_pointer)
{
    ZwpRelativePointerManagerV1 manager{a_client};
    ZwpRelativePointerV1 pointer{manager, a_client.the_pointer()};

    EXPECT_THAT(pointer, NotNull());
}
