/*
 * Copyright © 2021 Canonical Ltd.
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

#ifndef MOCK_INPUT_METHOD_V2
#define MOCK_INPUT_METHOD_V2

#include "generated/wayland-client.h"
#include "generated/input-method-unstable-v2-client.h"

#include "in_process_server.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"
#include "method_event_impl.h"

#include <gmock/gmock.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_method_manager_v2)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_method_v2)

class MockInputMethodV2 : public WlHandle<zwp_input_method_v2>
{
private:
    void done_wrapper()
    {
        done_count_++;
        done();
    }

public:
    MockInputMethodV2(zwp_input_method_v2* proxy)
        : WlHandle{proxy}
    {
        zwp_input_method_v2_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD0(activate, void());
    MOCK_METHOD0(deactivate, void());
    MOCK_METHOD3(surrounding_text, void(std::string const&, uint32_t, uint32_t));
    MOCK_METHOD1(text_change_cause, void(uint32_t));
    MOCK_METHOD2(content_type, void(uint32_t, uint32_t));
    MOCK_METHOD0(done, void());
    MOCK_METHOD0(unavailable, void());

    auto done_count() -> uint32_t { return done_count_; }

    static zwp_input_method_v2_listener constexpr listener {
        method_event_impl<&MockInputMethodV2::activate>,
        method_event_impl<&MockInputMethodV2::deactivate>,
        method_event_impl<&MockInputMethodV2::surrounding_text>,
        method_event_impl<&MockInputMethodV2::text_change_cause>,
        method_event_impl<&MockInputMethodV2::content_type>,
        method_event_impl<&MockInputMethodV2::done_wrapper>,
        method_event_impl<&MockInputMethodV2::unavailable>,
    };

private:
    uint32_t done_count_{0};
};
}

#endif // MOCK_INPUT_METHOD_V2
