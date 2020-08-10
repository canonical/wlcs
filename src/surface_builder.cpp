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

#include "surface_builder.h"

auto wlcs::SurfaceBuilder::all_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>
{
    // All this pointer casting nonsense is only to make 16.04 GCC happy
    return {
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<WlShellSurfaceBuilder>()),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<XdgV6SurfaceBuilder>()),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<XdgStableSurfaceBuilder>()),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<SubsurfaceBuilder>(std::make_pair(0, 0))),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<SubsurfaceBuilder>(std::make_pair(7, 12)))};
}

auto wlcs::SurfaceBuilder::toplevel_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>
{
    // All this pointer casting nonsense is only to make 16.04 GCC happy
    return {
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<WlShellSurfaceBuilder>()),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<XdgV6SurfaceBuilder>()),
        std::static_pointer_cast<SurfaceBuilder>(std::make_shared<XdgStableSurfaceBuilder>())};
}

auto wlcs::WlShellSurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(
        client.create_wl_shell_surface(size.first, size.second));
    server.move_surface_to(*surface, position.first, position.second);
    return surface;
}

auto wlcs::XdgV6SurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(
        client.create_xdg_shell_v6_surface(size.first, size.second));
    server.move_surface_to(*surface, position.first, position.second);
    return surface;
}

auto wlcs::XdgStableSurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(
        client.create_xdg_shell_stable_surface(size.first, size.second));
    server.move_surface_to(*surface, position.first, position.second);
    return surface;
}

wlcs::SubsurfaceBuilder::SubsurfaceBuilder(std::pair<int, int> offset)
    : SurfaceBuilder{
        "subsurface (offset " +
        std::to_string(offset.first) +
        ", " +
        std::to_string(offset.second) +
        ")"},
        offset{offset}
{
}

auto wlcs::SubsurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto main_surface = std::make_shared<wlcs::Surface>(
        client.create_visible_surface(80, 50));
    server.move_surface_to(
        *main_surface,
        position.first - offset.first,
        position.second - offset.second);
    client.run_on_destruction(
        [main_surface]() mutable
        {
            main_surface.reset();
        });
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *main_surface,
        offset.first, offset.second,
        size.first, size.second));
    // if subsurface is sync, tests would have to commit the parent to modify it
    // this is inconvenient to do in a generic way, so we make it desync
    wl_subsurface_set_desync(*subsurface);
    return subsurface;
}

std::ostream& operator<<(std::ostream& out, std::shared_ptr<wlcs::SurfaceBuilder> const& param)
{
    return out << param->name;
}
