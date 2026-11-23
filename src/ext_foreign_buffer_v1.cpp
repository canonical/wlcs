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

#include "ext_foreign_buffer_v1.h"
#include "in_process_server.h"

wlcs::ExtForeignBufferSessionV1::ExtForeignBufferSessionV1(ext_foreign_buffer_session_v1* session)
    : session{session}
{
    static ext_foreign_buffer_session_v1_listener const listener = {
        .size = [](
            void* data,
            ext_foreign_buffer_session_v1*,
            uint32_t buffer_width,
            uint32_t buffer_height,
            uint32_t logical_width,
            uint32_t logical_height)
            {
                auto self = static_cast<ExtForeignBufferSessionV1*>(data);
                self->sizes_.push_back({buffer_width, buffer_height, logical_width, logical_height});
            },
        .buffer_invalidated = [](
            void* data,
            ext_foreign_buffer_session_v1*)
            {
                auto self = static_cast<ExtForeignBufferSessionV1*>(data);
                self->invalidated_count_++;
            },
        .stopped = [](
            void* data,
            ext_foreign_buffer_session_v1*)
            {
                auto self = static_cast<ExtForeignBufferSessionV1*>(data);
                self->stopped_count_++;
            },
    };
    ext_foreign_buffer_session_v1_add_listener(session, &listener, this);
}

auto wlcs::ExtForeignBufferSessionV1::get_buffer() -> WlHandle<wl_buffer>
{
    return wrap_wl_object(ext_foreign_buffer_session_v1_get_buffer(session));
}

void wlcs::ExtForeignBufferSessionV1::dispatch_until_size(Client& client, size_t count)
{
    client.dispatch_until([this, count] { return sizes_.size() >= count; });
}

auto wlcs::operator<<(std::ostream& out, ExtForeignBufferSessionV1::SizeEvent const& size) -> std::ostream&
{
    return out << "{buffer " << size.buffer_width << "x" << size.buffer_height
               << ", logical " << size.logical_width << "x" << size.logical_height << "}";
}
