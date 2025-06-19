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

#ifndef WLCS_HELPERS_H_
#define WLCS_HELPERS_H_

#include <chrono>
#include <cstddef>
#include <memory>

struct WlcsServerIntegration;

namespace wlcs
{
namespace helpers
{
int create_anonymous_file(size_t size);

void set_command_line(int argc, char const** argv);

int get_argc();
char const** get_argv();

void set_entry_point(std::shared_ptr<WlcsServerIntegration const> const& entry_point);

std::shared_ptr<WlcsServerIntegration const> get_test_hooks();

/**
 * A short duration.
 *
 * Use this when you need to wait for something to happen in the success case
 * (that you have no way of monitoring otherwise), like verifying that an action
 * did *not* change a window property.
 */
auto a_short_time() -> std::chrono::seconds;

/**
 * A long duration.
 *
 * Use this where you're waiting for something to happen and need a timeout
 * to determine when to give up waiting, like committing a buffer to a surface
 * and waiting for the previous buffer to be released.
 */
auto a_long_time() -> std::chrono::seconds;
}
}

#endif //WLCS_HELPERS_H_
