/*
 * Copyright © 2025 Canonical Ltd.
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

#include "in_process_server.h"
#include "linux_dmabuf_v1.h"

#include <gmock/gmock.h>

class LinuxDmabufTest
    : public wlcs::StartedInProcessServer
{
};

TEST_F(LinuxDmabufTest, default_feedback)
{
    wlcs::Client client{the_server()};

    wlcs::LinuxDmabufV1 linux_dmabuf{client};

    auto feedback = linux_dmabuf.get_default_feedback();
    EXPECT_CALL(*feedback, format_table(testing::_, testing::_));
    EXPECT_CALL(*feedback, main_device(testing::_));
    EXPECT_CALL(*feedback, tranche_target_device(testing::_));
    EXPECT_CALL(*feedback, tranche_flags(testing::_));
    EXPECT_CALL(*feedback, tranche_formats(testing::_));
    EXPECT_CALL(*feedback, tranche_done());
    EXPECT_CALL(*feedback, done());
    client.roundtrip();
}
