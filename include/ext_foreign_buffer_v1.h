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

#ifndef WLCS_EXT_FOREIGN_BUFFER_V1_H
#define WLCS_EXT_FOREIGN_BUFFER_V1_H

#include "generated/wayland-client.h"
#include "generated/ext-foreign-buffer-v1-client.h"
#include "wl_handle.h"
#include "wl_interface_descriptor.h"

#include <cstdint>
#include <ostream>
#include <vector>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_buffer_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_buffer_session_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(wl_buffer)

class Client;

class ExtForeignBufferSessionV1
{
public:
    /// The arguments of an ext_foreign_buffer_session_v1.size event
    struct SizeEvent
    {
        uint32_t buffer_width;
        uint32_t buffer_height;
        uint32_t logical_width;
        uint32_t logical_height;

        auto operator==(SizeEvent const&) const -> bool = default;
    };

    explicit ExtForeignBufferSessionV1(ext_foreign_buffer_session_v1* session);
    ExtForeignBufferSessionV1(ExtForeignBufferSessionV1 const&) = delete;
    ExtForeignBufferSessionV1& operator=(ExtForeignBufferSessionV1 const&) = delete;

    /// Sends get_buffer. The returned buffer is owned by the caller.
    auto get_buffer() -> WlHandle<wl_buffer>;

    /// Dispatches \p client until at least \p count size events have been received.
    void dispatch_until_size(Client& client, size_t count = 1);

    auto sizes() const -> std::vector<SizeEvent> const& { return sizes_; }
    auto invalidated_count() const { return invalidated_count_; }
    auto stopped_count() const { return stopped_count_; }

    operator ext_foreign_buffer_session_v1*() const { return session; }

private:
    WlHandle<ext_foreign_buffer_session_v1> const session;

    std::vector<SizeEvent> sizes_;
    int invalidated_count_{0};
    int stopped_count_{0};
};

auto operator<<(std::ostream& out, ExtForeignBufferSessionV1::SizeEvent const& size) -> std::ostream&;
}

#endif /* WLCS_EXT_FOREIGN_BUFFER_V1_H */
