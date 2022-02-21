/*
 * Copyright © 2019 Canonical Ltd.
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

#ifndef XDG_OUTPUT_V1_H
#define XDG_OUTPUT_V1_H

#include <experimental/optional>

#include "generated/wayland-client.h"
#include "generated/xdg-output-unstable-v1-client.h"

#include "in_process_server.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"

#include <memory>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zxdg_output_manager_v1)

class XdgOutputManagerV1
{
public:
    XdgOutputManagerV1(Client& client);
    ~XdgOutputManagerV1();

    operator zxdg_output_manager_v1*() const;
    auto client() const -> Client&;

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

class XdgOutputV1
{
public:
    struct State
    {
        std::experimental::optional<std::pair<int, int>> logical_position;
        std::experimental::optional<std::pair<int, int>> logical_size;
        std::experimental::optional<std::string> name;
        std::experimental::optional<std::string> description;
    };

    XdgOutputV1(XdgOutputManagerV1& manager, size_t output_index);

    XdgOutputV1(XdgOutputV1 const&) = delete;
    XdgOutputV1 const& operator=(XdgOutputV1 const&) = delete;

    operator zxdg_output_v1*() const;
    auto state() -> State const&;

private:
    struct Impl;
    std::shared_ptr<Impl> const impl; // shared instead of unique because interally a weak is needed
};

}

#endif // XDG_OUTPUT_V1_H
