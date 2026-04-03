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

#include "expect_protocol_error.h"
#include "generated/ext-input-trigger-action-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

using namespace wlcs;
using testing::Eq;

namespace wlcs {
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_manager_v1)
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_v1)
}

class ExtInputTriggerActionV1Test : public wlcs::StartedInProcessServer
{
public:
    Client a_client{the_server()};
    WlHandle<ext_input_trigger_action_manager_v1> action_manager{
        a_client.bind_if_supported<ext_input_trigger_action_manager_v1>(AnyVersion)};
};

TEST_F(ExtInputTriggerActionV1Test, get_input_trigger_action_with_invalid_token_raises_protocol_error)
{
    // Hold onto the object until the end of the test.
    auto const wrapper = wrap_wl_object(
        ext_input_trigger_action_manager_v1_get_input_trigger_action(
            action_manager,
            "invalid-token-that-was-not-issued-by-the-compositor"));

    EXPECT_PROTOCOL_ERROR(
        { a_client.roundtrip(); },
        &ext_input_trigger_action_manager_v1_interface,
        EXT_INPUT_TRIGGER_ACTION_MANAGER_V1_ERROR_INVALID_TOKEN);
}
