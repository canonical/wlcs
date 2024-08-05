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
 */

#include "fractional_scale_v1.h"

#include "expect_protocol_error.h"
#include "in_process_server.h"

using namespace wlcs;
using testing::Eq;

class FractionalScaleV1Test : public wlcs::StartedInProcessServer
{
public:
    Client a_client{the_server()};
    WpFractionalScaleManagerV1 manager{a_client};
    Surface a_surface{a_client};
};

TEST_F(FractionalScaleV1Test, fractional_scale_creation_does_not_throw)
{
    WpFractionalScaleV1 fractional_scale{manager, a_surface};

    EXPECT_NO_THROW(a_client.roundtrip());
}

TEST_F(FractionalScaleV1Test, duplicate_fractional_scales_throw_fractional_scale_exists)
{
    WpFractionalScaleV1 fractional_scale1{manager, a_surface};
    WpFractionalScaleV1 fractional_scale2{manager, a_surface};

    EXPECT_PROTOCOL_ERROR(
        { a_client.roundtrip(); },
        &wp_fractional_scale_manager_v1_interface,
        WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS);
}
