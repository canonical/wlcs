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
#include "version_specifier.h"
#include "wl_interface_descriptor.h"
#include "generated/xdg-activation-v1-client.h"
#include "xdg_shell_v6.h"

#include <gmock/gmock.h>

using namespace testing;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(xdg_activation_v1)
}

class XdgActivationV1Test : public wlcs::StartedInProcessServer
{
public:
    XdgActivationV1Test()
        : client{the_server()},
          manager{client.bind_if_supported<xdg_activation_v1>(wlcs::AnyVersion)}
    {}

    struct DoneEvent
    {
        bool received = false;
        std::string token;
    };

    wlcs::Client client;
    wlcs::WlHandle<xdg_activation_v1> manager;
};

TEST_F(XdgActivationV1Test, done_event_is_received_following_commit)
{
    DoneEvent done_event;
    auto surface1 = client.create_visible_surface(400, 400);
    client.roundtrip();

    xdg_activation_token_v1* token = xdg_activation_v1_get_activation_token(manager);

    xdg_activation_token_v1_listener listener = {
        [](void* data, struct xdg_activation_token_v1*, const char* token)
        {
            auto const event = static_cast<DoneEvent*>(data);
            event->received = true;
            event->token = token;
        }
    };

    xdg_activation_token_v1_add_listener(token, &listener, &done_event);
    xdg_activation_token_v1_commit(token);
    client.roundtrip();
    client.dispatch_until([&] { return done_event.received; });
}

TEST_F(XdgActivationV1Test, requested_surface_is_activated)
{
    // First, create the surface that we'll want to activate later
    wlcs::Surface to_activate{client};
    wlcs::XdgSurfaceV6 xdg_surface(client, to_activate);
    EXPECT_CALL(xdg_surface, configure).WillOnce([&](uint32_t serial)
         {
             zxdg_surface_v6_ack_configure(xdg_surface, serial);
         });
    wlcs::XdgToplevelV6 toplevel{xdg_surface};
    to_activate.attach_buffer(600, 400);
    client.roundtrip();

    // Then, create the surface that we're going to use as the activator
    wlcs::Surface activator{client};
    activator.attach_buffer(600, 400);
    client.roundtrip();

    // Then get a token
    DoneEvent done_event;
    xdg_activation_token_v1* token = xdg_activation_v1_get_activation_token(manager);
    xdg_activation_token_v1_listener listener = {
        [](void* data, struct xdg_activation_token_v1*, const char* token)
        {
            auto const event = static_cast<DoneEvent*>(data);
            event->received = true;
            event->token = token;
        }
    };
    xdg_activation_token_v1_add_listener(token, &listener, &done_event);
    xdg_activation_token_v1_commit(token);
    client.roundtrip();
    client.dispatch_until([&] { return done_event.received; });

    // Finally, activate [to_activate]
    bool is_activated = false;
    zxdg_toplevel_v6_listener xdg_toplevel_listener = {
        .configure=[](void* data,
          struct zxdg_toplevel_v6*,
          int32_t,
          int32_t,
          struct wl_array* state)
        {
            auto is_activated = static_cast<bool*>(data);
            for (auto item = static_cast<zxdg_toplevel_v6_state*>(state->data);
                 (char*)item < static_cast<char*>(state->data) + state->size;
                 item++)
            {
                switch (*item)
                {
                    case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
                        *is_activated = true;
                        break;
                    default:
                        break;
                }
            }
            return;
        },
        .close=[](void*, struct zxdg_toplevel_v6*){}
    };

    zxdg_toplevel_v6_add_listener(toplevel.toplevel, &xdg_toplevel_listener, &is_activated);
    xdg_activation_v1_activate(manager, done_event.token.c_str(), to_activate);
    client.roundtrip();
    client.dispatch_until([&] { return is_activated; });
}