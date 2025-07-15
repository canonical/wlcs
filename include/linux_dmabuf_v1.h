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

    MOCK_METHOD0(done, void());
    MOCK_METHOD2(format_table, void(int fd, uint32_t size));
    MOCK_METHOD1(main_device, void(dev_t devnum));
    MOCK_METHOD0(tranche_done, void());
    MOCK_METHOD1(tranche_target_device, void(dev_t devnum));
    MOCK_METHOD1(tranche_formats, void(std::vector<uint32_t> indices));
    MOCK_METHOD1(tranche_flags, void(uint32_t flags));

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

class LinuxDmabufV1
{
public:
    LinuxDmabufV1(Client& client);
    LinuxDmabufV1(LinuxDmabufV1 const&) = delete;
    LinuxDmabufV1& operator=(LinuxDmabufV1 const&) = delete;
    ~LinuxDmabufV1();

    std::shared_ptr<LinuxDmabufFeedbackV1> get_default_feedback();

private:
    struct Impl;
    std::unique_ptr<Impl> const impl;
};

}

#endif // WLCS_LINUX_DMABUF_V1_
