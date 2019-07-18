/*
 * Copyright © 2018 Canonical Ltd.
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

#include "foreign_toplevel_management_v1.h"
#include "version_specifier.h"

wlcs::ForeignToplevelManager::ForeignToplevelManager(wlcs::Client& client)
    : manager{client.bind_if_supported<zwlr_foreign_toplevel_manager_v1>(AnyVersion)}
{
    if (!manager)
        throw std::runtime_error("zwlr_foreign_toplevel_manager_v1 not supported by compositor");
    static zwlr_foreign_toplevel_manager_v1_listener const listener = {
        +[] /* toplevel */ (
            void* data,
            zwlr_foreign_toplevel_manager_v1*,
            zwlr_foreign_toplevel_handle_v1* toplevel)
            {
                auto self = static_cast<ForeignToplevelManager*>(data);
                self->toplevels_.push_back(std::make_unique<ForeignToplevelHandle>(self, toplevel));
            },
        +[] /* finished */ (
            void* /*data*/,
            zwlr_foreign_toplevel_manager_v1 *)
            {
            }
    };
    zwlr_foreign_toplevel_manager_v1_add_listener(manager, &listener, this);
}

wlcs::ForeignToplevelManager::~ForeignToplevelManager()
{
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}

wlcs::ForeignToplevelHandle::ForeignToplevelHandle(
    ForeignToplevelManager* manager,
    zwlr_foreign_toplevel_handle_v1* handle)
    : handle{handle},
      manager{manager}
{
    static zwlr_foreign_toplevel_handle_v1_listener const listener = {
        +[] /* title */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            char const* title)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                self->title_ = title;
                self->dirty_ = true;
            },
        +[] /* app_id */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            char const* app_id)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                self->app_id_ = app_id;
                self->dirty_ = true;
            },
        +[] /* output_enter */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_output* output)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                self->outputs_.push_back(output);
                self->dirty_ = true;
            },
        +[] /* output_leave */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_output* output)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                std::remove(
                    self->outputs_.begin(),
                    self->outputs_.end(),
                    output);
                self->dirty_ = true;
            },
        +[] /*state */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*,
            wl_array* state)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);

                self->maximized_ = false;
                self->minimized_ = false;
                self->activated_ = false;
                self->fullscreen_ = false;

                for (auto item = static_cast<zwlr_foreign_toplevel_handle_v1_state*>(state->data);
                    (char*)item < static_cast<char*>(state->data) + state->size;
                    item++)
                {
                    switch (*item)
                    {
                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                        self->maximized_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                        self->minimized_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                        self->activated_ = true;
                        break;

                    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                        self->fullscreen_ = true;
                        break;
                    }
                }
                self->dirty_ = true;
            },
        +[] /* done */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                self->dirty_ = false;
            },
        +[] /* closed */ (
            void* data,
            zwlr_foreign_toplevel_handle_v1*)
            {
                auto self = static_cast<ForeignToplevelHandle*>(data);
                self->destroy();
            }
    };
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

wlcs::ForeignToplevelHandle::~ForeignToplevelHandle()
{
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

void wlcs::ForeignToplevelHandle::destroy()
{
    auto& toplevels = manager->toplevels_;
    toplevels.erase(
        std::remove_if(
            toplevels.begin(),
            toplevels.end(),
            [this](std::unique_ptr<ForeignToplevelHandle> const& toplevel)
            {
                return toplevel->handle == handle;
            }),
        toplevels.end()
    );
}
