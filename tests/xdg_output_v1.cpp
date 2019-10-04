/*
 * Copyright © 2019 Canonical Ltd.
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

#include "in_process_server.h"
#include "xdg_output_v1.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

class XdgOutputV1Test
    : public wlcs::StartedInProcessServer
{
private:
    wlcs::CheckInterfaceExpected expect_xdg_output_supported{the_server(), zxdg_output_manager_v1_interface};
};

TEST_F(XdgOutputV1Test, xdg_output_properties_set)
{
    Client client{the_server()};

    ASSERT_THAT(client.output_count(), Ge(1u));

    XdgOutputV1 xdg_output{client, 0};
    client.roundtrip();

    auto const& state = xdg_output.state();
    ASSERT_THAT((bool)state.logical_position, true);
    ASSERT_THAT((bool)state.logical_size, true);
    ASSERT_THAT((bool)state.name, true);
    // Description is optional
}
