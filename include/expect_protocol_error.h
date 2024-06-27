/*
 * Copyright © 2017-Canonical Ltd.
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

#ifndef WLCS_EXPECT_PROTOCOL_ERROR_H_
#define WLCS_EXPECT_PROTOCOL_ERROR_H_

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define EXPECT_PROTOCOL_ERROR(block, iface, err_code) \
    try { \
        block \
        FAIL() << "Expected protocol error not raised"; \
    } catch (wlcs::ProtocolError const& err) {\
        EXPECT_THAT(err.interface(), Eq(iface));\
        EXPECT_THAT(err.error_code(), Eq(err_code));\
    }

#endif // WLCS_EXPECT_PROTOCOL_ERROR_H_
