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

#ifndef WLCS_EXT_FOREIGN_TOPLEVEL_LIST_V1_H
#define WLCS_EXT_FOREIGN_TOPLEVEL_LIST_V1_H

#include "generated/ext-foreign-toplevel-list-v1-client.h"
#include "wl_interface_descriptor.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_toplevel_list_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_toplevel_handle_v1)

class Client;

class ExtForeignToplevelHandleV1
{
public:
    ExtForeignToplevelHandleV1(ext_foreign_toplevel_handle_v1* handle);
    ~ExtForeignToplevelHandleV1();

    auto is_dirty() const -> bool;
    auto closed() const -> bool;
    auto title() const -> std::optional<std::string>;
    auto app_id() const -> std::optional<std::string>;
    auto identifier() const -> std::optional<std::string>;

    operator ext_foreign_toplevel_handle_v1*() const;

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

class ExtForeignToplevelListV1
{
public:
    ExtForeignToplevelListV1(wlcs::Client& client);
    ~ExtForeignToplevelListV1();

    auto toplevels() const -> std::vector<std::unique_ptr<ExtForeignToplevelHandleV1>> const&;
    auto toplevel() const -> ExtForeignToplevelHandleV1 const&;
    auto toplevel(std::string const& app_id) const -> ExtForeignToplevelHandleV1 const&;
    void remove(ExtForeignToplevelHandleV1 const& toplevel);

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

}

#endif /* WLCS_EXT_FOREIGN_TOPLEVEL_LIST_V1_H */
