/*
 * Copyright © 2025 Canonical Ltd.
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

#include "linux_dmabuf_v1.h"
#include "version_specifier.h"

struct wlcs::LinuxDmabufFeedbackV1::Impl
{
    Impl(zwp_linux_dmabuf_feedback_v1 *feedback)
        : feedback{feedback}
    {
    }

    WlHandle<zwp_linux_dmabuf_feedback_v1> const feedback;
};

wlcs::LinuxDmabufFeedbackV1::LinuxDmabufFeedbackV1(struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
    static zwp_linux_dmabuf_feedback_v1_listener const listener
    {
        [] /* done */ (
            void* data,
            struct zwp_linux_dmabuf_feedback_v1 *)
            {
                static_cast<LinuxDmabufFeedbackV1*>(data)->done();
            },
        [] /* format_table */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *,
            int32_t fd,
            uint32_t size)
            {
                static_cast<LinuxDmabufFeedbackV1*>(data)->format_table(fd, size);
            },
        [] /* main_device */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *,
            struct wl_array *device)
            {
                dev_t devnum = device->size == 1 ? *static_cast<dev_t*>(device->data) : 0;
                static_cast<LinuxDmabufFeedbackV1*>(data)->main_device(devnum);
            },
        [] /* tranche_done */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *)
            {
                static_cast<LinuxDmabufFeedbackV1*>(data)->tranche_done();
            },
        [] /* tranche_target_device */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *,
            struct wl_array *device)
            {
                dev_t devnum = device->size == 1 ? *static_cast<dev_t*>(device->data) : 0;
                static_cast<LinuxDmabufFeedbackV1*>(data)->tranche_target_device(devnum);
            },
        [] /* tranche_formats */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *,
            struct wl_array *indices)
            {
                auto i = static_cast<uint32_t*>(indices->data);
                static_cast<LinuxDmabufFeedbackV1*>(data)->tranche_formats(std::vector<uint32_t>(i, i + (indices->size/sizeof(uint32_t))));
            },
        [] /* tranche_flags */ (
            void *data,
            struct zwp_linux_dmabuf_feedback_v1 *,
            uint32_t flags)
            {
                static_cast<LinuxDmabufFeedbackV1*>(data)->tranche_flags(flags);
            }
    };
    zwp_linux_dmabuf_feedback_v1_add_listener(feedback, &listener, this);
}

wlcs::LinuxDmabufFeedbackV1::~LinuxDmabufFeedbackV1() = default;
