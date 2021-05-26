/*
 * Copyright © 2020 Canonical Ltd.
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

#ifndef WLCS_SURFACE_BUILDER_H_
#define WLCS_SURFACE_BUILDER_H_

#include <string>
#include <vector>
#include "in_process_server.h"

namespace wlcs
{

struct SurfaceBuilder
{
    SurfaceBuilder(std::string const& name) : name{name} {}
    virtual ~SurfaceBuilder() = default;

    virtual auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> = 0;

    std::string const name;

    static auto all_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>;
    static auto toplevel_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>;

    static auto surface_builder_to_string(
        testing::TestParamInfo<std::shared_ptr<SurfaceBuilder>> builder) -> std::string;
};

struct WlShellSurfaceBuilder : SurfaceBuilder
{
    WlShellSurfaceBuilder() : SurfaceBuilder{"wl_shell_surface"} {}

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override;
};

struct XdgV6SurfaceBuilder : SurfaceBuilder
{
    XdgV6SurfaceBuilder() : SurfaceBuilder{"zxdg_surface_v6"} {}

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override;
};

struct XdgStableSurfaceBuilder : SurfaceBuilder
{
    XdgStableSurfaceBuilder(int left_offset, int top_offset, int right_offset, int bottom_offset);

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override;

    int left_offset, top_offset, right_offset, bottom_offset;
};

struct SubsurfaceBuilder : SurfaceBuilder
{
    SubsurfaceBuilder(std::pair<int, int> offset);

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override;

    std::pair<int, int> offset;
};

// TODO: popup surfaces
}

namespace std
{
std::ostream& operator<<(std::ostream& out, std::shared_ptr<wlcs::SurfaceBuilder> const& param);
}

#endif // WLCS_SURFACE_BUILDER_H_
