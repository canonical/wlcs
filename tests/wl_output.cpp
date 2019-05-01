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

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

using WlOutputTest = wlcs::InProcessServer;

TEST_F(WlOutputTest, wl_output_properties_set)
{
    wlcs::Client client{the_server()};

    ASSERT_THAT(client.output_count(), Eq(1u));
    auto output = client.output_state(0);

    EXPECT_THAT(output.geometry_set, Eq(true));
    EXPECT_THAT(output.mode_set, Eq(true));
    EXPECT_THAT(output.scale_set, Eq(true));

    EXPECT_THAT(output.scale, Eq(1));
}
