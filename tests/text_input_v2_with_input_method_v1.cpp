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

#include "in_process_server.h"
#include <gmock/gmock.h>
#include "mock_text_input_v2.h"
#include "mock_input_method_v1.h"

using namespace testing;

struct TextInputV2WithInputMethodV1Test : wlcs::StartedInProcessServer
{
    TextInputV2WithInputMethodV1Test()
        : StartedInProcessServer{},
          pointer{the_server().create_pointer()},
          app_client{the_server()},
          input_client{the_server()}
    {
    }

    wlcs::Pointer pointer;
    wlcs::Client app_client;
    std::optional<wlcs::Surface> app_surface;
    wlcs::Client input_client;
};