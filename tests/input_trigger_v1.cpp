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
 *
 * Authored by: Tarek Ismail <tarek.ismail@canonical.com>
 */


#include "generated/ext-input-trigger-registration-v1-client.h"
#include "generated/ext-input-trigger-action-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

#include "gmock/gmock.h"
#include <xkbcommon/xkbcommon.h>

using namespace testing;
using namespace wlcs;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_registration_manager_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_manager_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_control_v1);
}

class TestInputTrigger: public StartedInProcessServer
{

};

TEST_F(TestInputTrigger, foo)
{
    Client client{the_server()};

    auto const registration_manager =
        client.bind_if_supported<ext_input_trigger_registration_manager_v1>(AtLeastVersion{1});
    auto const action_manager = client.bind_if_supported<ext_input_trigger_action_manager_v1>(AtLeastVersion{1});

    auto const trigger = ext_input_trigger_registration_manager_v1_register_keyboard_sym_trigger(
        registration_manager,
        EXT_INPUT_TRIGGER_REGISTRATION_MANAGER_V1_MODIFIERS_SHIFT |
            EXT_INPUT_TRIGGER_REGISTRATION_MANAGER_V1_MODIFIERS_CTRL,
        XKB_KEY_C);
    ext_input_trigger_v1_listener const trigger_listener = {
        .done =
            [](void* data, auto* trigger)
        {
            (void)data;
            (void)trigger;
        },
        .failed =
            [](auto...)
        {
            FAIL() << "Unexpected call to ext_input_trigger_v1.failed";
        },
    };
    ext_input_trigger_v1_add_listener(trigger, &trigger_listener, nullptr);

    std::string action_control_token{};
    auto const action_control =
        ext_input_trigger_registration_manager_v1_get_action_control(registration_manager, "ctrl_alt_c");
    ext_input_trigger_action_control_v1_listener const action_control_listener = {
        .done = [](void* data, auto* action_control, char const* token)
        {
            (void)action_control;
            auto* action_control_token = static_cast<std::string*>(data);
            *action_control_token = std::string{token};
        }};
    ext_input_trigger_action_control_v1_add_listener(action_control, &action_control_listener, &action_control_token);

    ext_input_trigger_action_control_v1_add_input_trigger_event(action_control, trigger);

    // wait for done event with token
    client.roundtrip();

    // Make sure that we get a token
    EXPECT_THAT(action_control_token, Not(StrEq("")));

    // get corresponding action
    auto const action =
        ext_input_trigger_action_manager_v1_get_input_trigger_action(action_manager, action_control_token.c_str());

    EXPECT_THAT(action, Ne(nullptr));
}
