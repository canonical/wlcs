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

    MOCK_METHOD2(enter, void(uint32_t, wl_surface *));
    MOCK_METHOD2(leave, void(uint32_t, wl_surface *));
    MOCK_METHOD5(input_panel_state, void(uint32_t, int32_t, int32_t, int32_t, int32_t));
    MOCK_METHOD2(preedit_string, void(std::string const&, std::string const&));
    MOCK_METHOD3(predit_styling, void(uint32_t, uint32_t, uint32_t));
    MOCK_METHOD1(preedit_cursor, void(int32_t));
    MOCK_METHOD1(commit_string, void(std::string const&));
    MOCK_METHOD2(cursor_position, void(int32_t, int32_t));
    MOCK_METHOD2(delete_surrounding_text, void(uint32_t, uint32_t));
    MOCK_METHOD1(modifiers_map, void(wl_array*));
    MOCK_METHOD4(keysym, void(uint32_t, uint32_t, uint32_t, uint32_t));
    MOCK_METHOD1(language, void(std::string const&));
    MOCK_METHOD1(text_direction, void(uint32_t));
    MOCK_METHOD2(configure_surrounding_text, void(int32_t, int32_t));
    MOCK_METHOD2(input_method_changed, void(uint32_t, uint32_t));

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
};

}

#endif
