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

#ifndef WLCS_FOREIGN_TOPLEVEL_MANAGEMENT_V1_H
#define WLCS_FOREIGN_TOPLEVEL_MANAGEMENT_V1_H

#include "in_process_server.h"
#include "wl_handle.h"
#include "generated/wlr-foreign-toplevel-management-unstable-v1-client.h"

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_foreign_toplevel_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_foreign_toplevel_handle_v1)

class ForeignToplevelManager;

class ForeignToplevelHandle
{
public:
    ForeignToplevelHandle(
        ForeignToplevelManager* manager,
        zwlr_foreign_toplevel_handle_v1* handle);
    ForeignToplevelHandle(ForeignToplevelHandle const&) = delete;
    ForeignToplevelHandle& operator=(ForeignToplevelHandle const&) = delete;
    ~ForeignToplevelHandle();

    operator zwlr_foreign_toplevel_handle_v1*() const { return handle; }
    auto is_dirty() const { return dirty_; }
    auto title() const { return title_; }
    auto app_id() const { return app_id_; }
    auto outputs() const -> std::vector<wl_output*> const& { return outputs_; }
    auto maximized() const { return maximized_; }
    auto minimized() const { return minimized_; }
    auto activated() const { return activated_; }
    auto fullscreen() const { return fullscreen_; }

private:
    // When we receive the .close event and need to delete ourselves
    void destroy();

    WlHandle<zwlr_foreign_toplevel_handle_v1> const handle;
    ForeignToplevelManager* const manager;

    bool dirty_{false};
    std::experimental::optional<std::string> title_;
    std::experimental::optional<std::string> app_id_;
    std::vector<wl_output*> outputs_;
    bool maximized_{false}, minimized_{false}, activated_{false}, fullscreen_{false};
};

class ForeignToplevelManager
{
public:
    ForeignToplevelManager(wlcs::Client& client);
    ForeignToplevelManager(ForeignToplevelManager const&) = delete;
    ForeignToplevelManager& operator=(ForeignToplevelManager const&) = delete;
    ~ForeignToplevelManager();

    operator zwlr_foreign_toplevel_manager_v1*() const { return manager; }
    auto toplevels() const -> std::vector<std::unique_ptr<ForeignToplevelHandle>> const& { return toplevels_; }

private:
    friend ForeignToplevelHandle;

    WlHandle<zwlr_foreign_toplevel_manager_v1> const manager;
    std::vector<std::unique_ptr<ForeignToplevelHandle>> toplevels_;
};

}

#endif // WLCS_FOREIGN_TOPLEVEL_MANAGEMENT_V1_H
