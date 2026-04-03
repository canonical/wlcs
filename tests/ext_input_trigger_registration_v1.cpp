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
#include "generated/ext-input-trigger-registration-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace wlcs;
using testing::_;

namespace wlcs {
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_registration_manager_v1)
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_v1)
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_control_v1)
}

class ExtInputTriggerListener
{
public:
    ExtInputTriggerListener(ext_input_trigger_v1* trigger)
        : proxy{trigger}
    {
        static const ext_input_trigger_v1_listener listener = {
            .done = [](void* data, ext_input_trigger_v1*)
                {
                    static_cast<ExtInputTriggerListener*>(data)->done();
                },
            .failed = [](void* data, ext_input_trigger_v1*)
                {
                    static_cast<ExtInputTriggerListener*>(data)->failed();
                },
        };
        ext_input_trigger_v1_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD(void, done, ());
    MOCK_METHOD(void, failed, ());

private:
    ext_input_trigger_v1* const proxy;
};

class ExtInputTriggerActionControlListener
{
public:
    ExtInputTriggerActionControlListener(ext_input_trigger_action_control_v1* control)
        : proxy{control}
    {
        static const ext_input_trigger_action_control_v1_listener listener = {
            .done = [](void* data, ext_input_trigger_action_control_v1*, const char* token)
                {
                    static_cast<ExtInputTriggerActionControlListener*>(data)->done(token);
                },
        };
        ext_input_trigger_action_control_v1_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD(void, done, (const char* token));

private:
    ext_input_trigger_action_control_v1* const proxy;
};

class ExtInputTriggerRegistrationListener
{
public:
    ExtInputTriggerRegistrationListener(ext_input_trigger_registration_manager_v1* manager)
        : proxy{manager}
    {
        static const ext_input_trigger_registration_manager_v1_listener listener = {
            .capabilities = [](
                void* data,
                ext_input_trigger_registration_manager_v1*,
                uint32_t capabilities)
                {
                    static_cast<ExtInputTriggerRegistrationListener*>(data)->capabilities(capabilities);
                },
        };
        ext_input_trigger_registration_manager_v1_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD(void, capabilities, (uint32_t capabilities));

private:
    ext_input_trigger_registration_manager_v1* const proxy;
};

class ExtInputTriggerRegistrationV1Test : public wlcs::StartedInProcessServer
{
public:
    Client a_client{the_server()};
    WlHandle<ext_input_trigger_registration_manager_v1> registration_manager{
        a_client.bind_if_supported<ext_input_trigger_registration_manager_v1>(AnyVersion)};
};

TEST_F(ExtInputTriggerRegistrationV1Test, manager_sends_capabilities_event)
{
    ExtInputTriggerRegistrationListener listener{registration_manager};

    EXPECT_CALL(listener, capabilities(_)).Times(1);
    a_client.roundtrip();
}

TEST_F(ExtInputTriggerRegistrationV1Test, can_register_keyboard_sym_trigger)
{
    auto trigger = wrap_wl_object(
        ext_input_trigger_registration_manager_v1_register_keyboard_sym_trigger(
            registration_manager,
            EXT_INPUT_TRIGGER_REGISTRATION_MANAGER_V1_MODIFIERS_META,
            0x0061)); // 'a' key

    ExtInputTriggerListener listener{trigger};

    // Protocol requires exactly one of done or failed to be sent
    bool done_called{false}, failed_called{false};
    ON_CALL(listener, done()).WillByDefault([&]{ done_called = true; });
    ON_CALL(listener, failed()).WillByDefault([&]{ failed_called = true; });
    EXPECT_CALL(listener, done()).Times(testing::AtMost(1));
    EXPECT_CALL(listener, failed()).Times(testing::AtMost(1));
    a_client.roundtrip();
    EXPECT_TRUE(done_called != failed_called) << "Expected exactly one of done or failed";
}

TEST_F(ExtInputTriggerRegistrationV1Test, can_register_keyboard_code_trigger)
{
    auto trigger = wrap_wl_object(
        ext_input_trigger_registration_manager_v1_register_keyboard_code_trigger(
            registration_manager,
            EXT_INPUT_TRIGGER_REGISTRATION_MANAGER_V1_MODIFIERS_CTRL,
            38)); // keycode for 'a'

    ExtInputTriggerListener listener{trigger};

    // Protocol requires exactly one of done or failed to be sent
    bool done_called{false}, failed_called{false};
    ON_CALL(listener, done()).WillByDefault([&]{ done_called = true; });
    ON_CALL(listener, failed()).WillByDefault([&]{ failed_called = true; });
    EXPECT_CALL(listener, done()).Times(testing::AtMost(1));
    EXPECT_CALL(listener, failed()).Times(testing::AtMost(1));
    a_client.roundtrip();
    EXPECT_TRUE(done_called != failed_called) << "Expected exactly one of done or failed";
}

TEST_F(ExtInputTriggerRegistrationV1Test, can_get_action_control)
{
    auto action_control = wrap_wl_object(
        ext_input_trigger_registration_manager_v1_get_action_control(
            registration_manager,
            "test-action"));

    ExtInputTriggerActionControlListener listener{action_control};

    EXPECT_CALL(listener, done(_)).Times(1);
    a_client.roundtrip();
}

TEST_F(ExtInputTriggerRegistrationV1Test, action_control_can_add_and_drop_triggers)
{
    auto trigger = wrap_wl_object(
        ext_input_trigger_registration_manager_v1_register_keyboard_sym_trigger(
            registration_manager,
            EXT_INPUT_TRIGGER_REGISTRATION_MANAGER_V1_MODIFIERS_META,
            0x0062)); // 'b' key

    auto action_control = wrap_wl_object(
        ext_input_trigger_registration_manager_v1_get_action_control(
            registration_manager,
            "test-action-with-trigger"));

    ExtInputTriggerActionControlListener listener{action_control};

    EXPECT_CALL(listener, done(_)).Times(1);
    a_client.roundtrip();

    // Add the trigger to the action
    ext_input_trigger_action_control_v1_add_input_trigger_event(action_control, trigger);
    EXPECT_NO_THROW(a_client.roundtrip());

    // Drop the trigger from the action
    ext_input_trigger_action_control_v1_drop_input_trigger_event(action_control, trigger);
    EXPECT_NO_THROW(a_client.roundtrip());
}
