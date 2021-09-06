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

#ifndef MOCK_TEXT_INPUT_V3_H
#define MOCK_TEXT_INPUT_V3_H

#include "generated/wayland-client.h"
#include "generated/text-input-unstable-v3-client.h"

#include "in_process_server.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"
#include "method_event_impl.h"

#include <memory>
#include <gmock/gmock.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_text_input_manager_v3)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_text_input_v3)

class MockTextInputV3 : public WlHandle<zwp_text_input_v3>
{
public:
    MockTextInputV3(zwp_text_input_v3* proxy)
        : WlHandle{proxy}
    {
        zwp_text_input_v3_add_listener(proxy, &listener, this);
    }

    MOCK_METHOD1(enter, void(wl_surface *));
    MOCK_METHOD1(leave, void(wl_surface *));
    MOCK_METHOD3(preedit_string, void(std::string const&, int32_t, int32_t));
    MOCK_METHOD1(commit_string, void(std::string const&));
    MOCK_METHOD2(delete_surrounding_text, void(int32_t, int32_t));
    MOCK_METHOD1(done, void(int32_t));

    static zwp_text_input_v3_listener constexpr listener {
        method_event_impl<&MockTextInputV3::enter>,
        method_event_impl<&MockTextInputV3::leave>,
        method_event_impl<&MockTextInputV3::preedit_string>,
        method_event_impl<&MockTextInputV3::commit_string>,
        method_event_impl<&MockTextInputV3::delete_surrounding_text>,
        method_event_impl<&MockTextInputV3::done>,
    };
};
}

#endif // MOCK_TEXT_INPUT_V3_H
