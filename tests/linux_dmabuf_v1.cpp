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
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

class LinuxDmabufTest
    : public wlcs::StartedInProcessServer
{
};

TEST_F(LinuxDmabufTest, default_feedback)
{
    wlcs::Client client{the_server()};

    auto linux_dmabuf = client.bind_if_supported<zwp_linux_dmabuf_v1>(AtLeastVersion{4});
    auto f = wrap_wl_object(zwp_linux_dmabuf_v1_get_default_feedback(linux_dmabuf));
    auto feedback = std::make_shared<LinuxDmabufFeedbackV1>(f);

    EXPECT_CALL(*feedback, format_table(_, _));
    EXPECT_CALL(*feedback, main_device(_));
    EXPECT_CALL(*feedback, tranche_target_device(_));
    EXPECT_CALL(*feedback, tranche_flags(_));
    EXPECT_CALL(*feedback, tranche_formats(_));
    EXPECT_CALL(*feedback, tranche_done());
    EXPECT_CALL(*feedback, done());
    client.roundtrip();
}
