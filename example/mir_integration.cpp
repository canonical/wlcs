/*
 * Copyright © 2017 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <fcntl.h>
#include "display_server.h"

#include "mir/fd.h"
#include "mir/server.h"
#include "mir/options/option.h"

#include "mir_test_framework/async_server_runner.h"
#include "mir_test_framework/headless_display_buffer_compositor_factory.h"
#include "mir_test_framework/executable_path.h"

namespace mtf = mir_test_framework;

void wlcs_server_start(WlcsDisplayServer* server)
{
    auto runner = reinterpret_cast<mtf::AsyncServerRunner*>(server);

    runner->start_server();
}

void wlcs_server_stop(WlcsDisplayServer* server)
{
    auto runner = reinterpret_cast<mtf::AsyncServerRunner*>(server);

    runner->stop_server();
}

WlcsDisplayServer* wlcs_create_server(int argc, char const** argv)
{
    auto runner = new mtf::AsyncServerRunner;
    runner->add_to_environment("MIR_SERVER_PLATFORM_GRAPHICS_LIB", mtf::server_platform("graphics-dummy.so").c_str());
    runner->add_to_environment("MIR_SERVER_PLATFORM_INPUT_LIB", mtf::server_platform("input-stub.so").c_str());
    runner->add_to_environment("MIR_SERVER_ENABLE_KEY_REPEAT", "false");
    runner->add_to_environment("MIR_SERVER_NO_FILE", "");
    runner->server.override_the_display_buffer_compositor_factory([]
    {
        return std::make_shared<mtf::HeadlessDisplayBufferCompositorFactory>();
    });
    runner->server.set_command_line(argc, argv);

    return reinterpret_cast<WlcsDisplayServer*>(runner);
}

void wlcs_destroy_server(WlcsDisplayServer* server)
{
    auto runner = reinterpret_cast<mtf::AsyncServerRunner*>(server);
    delete runner;
}

int wlcs_server_create_client_socket(WlcsDisplayServer* server)
{
    auto runner = reinterpret_cast<mtf::AsyncServerRunner*>(server);

    return fcntl(runner->server.open_wayland_client_socket(), F_DUPFD_CLOEXEC, 3);
}