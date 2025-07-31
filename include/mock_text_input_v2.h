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

#ifndef MOCK_TEXT_INPUT_V2_H
#define MOCK_TEXT_INPUT_V2_H

#include "generated/wayland-client.h"
#include "generated/text-input-unstable-v2-client.h"
#include "wl_handle.h"
#include "wl_interface_descriptor.h"
#include "method_event_impl.h"
#include <gmock/gmock.h>

namespace wlcs
{

WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_text_input_manager_v2)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_text_input_v2)

class MockTextInputV2 : public WlHandle<zwp_text_input_v2>
{
public:
    MockTextInputV2(zwp_text_input_v2* proxy)
        : WlHandle{proxy}
    {
        zwp_text_input_v2_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD(void, on_enter, (uint32_t, wl_surface*));
    MOCK_METHOD(void, on_leave, (uint32_t, wl_surface*));
    void enter(uint32_t in_serial, wl_surface* surface)
    {
        serial = in_serial;
        on_enter(in_serial, surface);
    }
    void leave(uint32_t in_serial, wl_surface* surface)
    {
        serial = in_serial;
        on_leave(in_serial, surface);
    }
    MOCK_METHOD(void, input_panel_state, (uint32_t, int32_t, int32_t, int32_t, int32_t));
    MOCK_METHOD(void, preedit_string, (std::string const&, std::string const&));
    MOCK_METHOD(void, predit_styling, (uint32_t, uint32_t, uint32_t));
    MOCK_METHOD(void, preedit_cursor, (int32_t));
    MOCK_METHOD(void, commit_string, (std::string const&));
    MOCK_METHOD(void, cursor_position, (int32_t, int32_t));
    MOCK_METHOD(void, delete_surrounding_text, (uint32_t, uint32_t));
    MOCK_METHOD(void, modifiers_map, (wl_array*));
    MOCK_METHOD(void, keysym, (uint32_t, uint32_t, uint32_t, uint32_t));
    MOCK_METHOD(void, language, (std::string const&));
    MOCK_METHOD(void, text_direction, (uint32_t));
    MOCK_METHOD(void, configure_surrounding_text, (int32_t, int32_t));
    MOCK_METHOD(void, on_input_method_changed, (uint32_t, uint32_t));
    void input_method_changed(uint32_t in_serial, uint32_t reason)
    {
        serial = in_serial;
        on_input_method_changed(in_serial, reason);
    }

    static zwp_text_input_v2_listener constexpr listener {
        method_event_impl<&MockTextInputV2::enter>,
        method_event_impl<&MockTextInputV2::leave>,
        method_event_impl<&MockTextInputV2::input_panel_state>,
        method_event_impl<&MockTextInputV2::preedit_string>,
        method_event_impl<&MockTextInputV2::predit_styling>,
        method_event_impl<&MockTextInputV2::preedit_cursor>,
        method_event_impl<&MockTextInputV2::commit_string>,
        method_event_impl<&MockTextInputV2::cursor_position>,
        method_event_impl<&MockTextInputV2::delete_surrounding_text>,
        method_event_impl<&MockTextInputV2::modifiers_map>,
        method_event_impl<&MockTextInputV2::keysym>,
        method_event_impl<&MockTextInputV2::language>,
        method_event_impl<&MockTextInputV2::text_direction>,
        method_event_impl<&MockTextInputV2::configure_surrounding_text>,
        method_event_impl<&MockTextInputV2::input_method_changed>
    };

    uint32_t serial;
};

}

#endif
