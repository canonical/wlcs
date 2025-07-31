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

#ifndef WLCS_LINUX_DMABUF_V1_
#define WLCS_LINUX_DMABUF_V1_

#include "in_process_server.h"
#include "generated/linux-dmabuf-stable-v1-client.h"

#include <gmock/gmock.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_linux_dmabuf_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_linux_dmabuf_feedback_v1)

class LinuxDmabufFeedbackV1
{
public:
    LinuxDmabufFeedbackV1(struct zwp_linux_dmabuf_feedback_v1 *feedback);
    LinuxDmabufFeedbackV1(LinuxDmabufFeedbackV1 const&) = delete;
    LinuxDmabufFeedbackV1& operator=(LinuxDmabufFeedbackV1 const&) = delete;
    ~LinuxDmabufFeedbackV1();

    MOCK_METHOD(void, done, ());
    MOCK_METHOD(void, format_table, (int fd, uint32_t size));
    MOCK_METHOD(void, main_device, (dev_t devnum));
    MOCK_METHOD(void, tranche_done, ());
    MOCK_METHOD(void, tranche_target_device, (dev_t devnum));
    MOCK_METHOD(void, tranche_formats, (std::vector<uint32_t> indices));
    MOCK_METHOD(void, tranche_flags, (uint32_t flags));

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

}

#endif // WLCS_LINUX_DMABUF_V1_
