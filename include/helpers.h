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
}
}

#endif //WLCS_HELPERS_H_
