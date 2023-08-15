/*
 * Copyright © Canonical Ltd.
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

#ifndef MOCK_INPUT_METHOD_V1_H
#define MOCK_INPUT_METHOD_V1_H

#include "generated/wayland-client.h"
#include "generated/input-method-unstable-v1-client.h"

#include "in_process_server.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"
#include "method_event_impl.h"

#include <gmock/gmock.h>
#include <memory>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_method_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_method_context_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_panel_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_input_panel_surface_v1)

class MockInputMethodContextV1 : public WlHandle<zwp_input_method_context_v1>
{
public:
    MockInputMethodContextV1(zwp_input_method_context_v1* proxy)
        : WlHandle{proxy}
    {
        zwp_input_method_context_v1_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD3(surrounding_text, void(std::string const&, uint32_t, uint32_t));
    MOCK_METHOD0(reset, void());
    MOCK_METHOD2(content_type, void(uint32_t, uint32_t));
    MOCK_METHOD2(invoke_action, void(uint32_t, uint32_t));
    MOCK_METHOD1(preferred_language, void(std::string const&));

    void commit_state(uint32_t in_serial)
    {
        serial = in_serial;
    }

    static zwp_input_method_context_v1_listener constexpr listener {
        method_event_impl<&MockInputMethodContextV1::surrounding_text>,
        method_event_impl<&MockInputMethodContextV1::reset>,
        method_event_impl<&MockInputMethodContextV1::content_type>,
        method_event_impl<&MockInputMethodContextV1::invoke_action>,
        method_event_impl<&MockInputMethodContextV1::commit_state>,
        method_event_impl<&MockInputMethodContextV1::preferred_language>
    };

    uint32_t serial = 0;
};

}

#endif
