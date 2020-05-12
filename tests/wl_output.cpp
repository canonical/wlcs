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

    ASSERT_THAT(client.output_count(), Ge(1u));
    auto output = client.output_state(0);

    EXPECT_THAT((bool)output.geometry_position, Eq(true));
    EXPECT_THAT((bool)output.mode_size, Eq(true));
    EXPECT_THAT((bool)output.scale, Eq(true));
}

TEST_F(WlOutputTest, wl_output_release)
{
    wlcs::Client client{the_server()};

    {
        // Acquire *any* wl_output; we don't care which
        auto output = client.bind_if_supported<wl_output>(
            wl_output_interface,
            wl_output_release,
            WL_OUTPUT_RELEASE_SINCE_VERSION);
        client.roundtrip();
    }
    // output is now released

    client.roundtrip();
}
