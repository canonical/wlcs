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

#include <gmock/gmock.h>

using namespace wlcs;
using testing::_;
using testing::AtLeast;

namespace wlcs {
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_manager_v1)
    WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_input_trigger_action_v1)
}

class ExtInputTriggerActionListener
{
public:
    ExtInputTriggerActionListener(ext_input_trigger_action_v1* action)
        : proxy{action}
    {
#define FORWARD_TO_MOCK(method) \
    [](void* data, ext_input_trigger_action_v1*, auto... args){ static_cast<ExtInputTriggerActionListener*>(data)->method(args...); }
        static const ext_input_trigger_action_v1_listener listener = {
            FORWARD_TO_MOCK(begin),
            FORWARD_TO_MOCK(update),
            FORWARD_TO_MOCK(end),
            FORWARD_TO_MOCK(unavailable),
        };
#undef FORWARD_TO_MOCK
        ext_input_trigger_action_v1_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD(void, begin, (uint32_t time, const char* activation_token));
    MOCK_METHOD(void, update, (uint32_t time, const char* activation_token, wl_fixed_t progress));
    MOCK_METHOD(void, end, (uint32_t time, const char* activation_token));
    MOCK_METHOD(void, unavailable, ());

private:
    ext_input_trigger_action_v1* const proxy;
};

class ExtInputTriggerActionV1Test : public wlcs::StartedInProcessServer
{
public:
    Client a_client{the_server()};
    WlHandle<ext_input_trigger_action_manager_v1> action_manager{
        a_client.bind_if_supported<ext_input_trigger_action_manager_v1>(AnyVersion)};
};

TEST_F(ExtInputTriggerActionV1Test, get_input_trigger_action_with_invalid_token_sends_unavailable)
{
    auto action = wrap_wl_object(
        ext_input_trigger_action_manager_v1_get_input_trigger_action(
            action_manager,
            "invalid-token-that-does-not-exist"));

    ExtInputTriggerActionListener listener{action};
    
    EXPECT_CALL(listener, unavailable()).Times(AtLeast(1));
    a_client.roundtrip();
}
