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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef WLCS_XFAIL_SUPPORTING_TEST_LISTENER_H_
#define WLCS_XFAIL_SUPPORTING_TEST_LISTENER_H_

#include <gtest/gtest.h>

#include <experimental/optional>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_set>

namespace testing
{
class XFailSupportingTestListenerWrapper : public testing::TestEventListener
{
public:
    explicit XFailSupportingTestListenerWrapper(std::unique_ptr<testing::TestEventListener>&& wrapped);

    void OnTestProgramStart(testing::UnitTest const& unit_test) override;

    void OnTestIterationStart(testing::UnitTest const& unit_test, int iteration) override;

    void OnEnvironmentsSetUpStart(testing::UnitTest const& unit_test) override;

    void OnEnvironmentsSetUpEnd(testing::UnitTest const& unit_test) override;

    void OnTestCaseStart(testing::TestCase const& test_case) override;

    void OnTestStart(testing::TestInfo const& test_info) override;

    void OnTestPartResult(testing::TestPartResult const& test_part_result) override;

    void OnTestEnd(testing::TestInfo const& test_info) override;
    void OnTestCaseEnd(testing::TestCase const& test_case) override;

    void OnEnvironmentsTearDownStart(testing::UnitTest const& unit_test) override;

    void OnEnvironmentsTearDownEnd(testing::UnitTest const& unit_test) override;

    void OnTestIterationEnd(testing::UnitTest const& unit_test, int iteration) override;

    void OnTestProgramEnd(testing::UnitTest const& unit_test) override;

    bool failed() const;
private:
    std::unique_ptr<testing::TestEventListener> const delegate;

    std::chrono::steady_clock::time_point current_test_start;
    ::testing::TestInfo const* current_test_info;
    std::experimental::optional<std::vector<std::string>> current_skip_reasons;

    std::unordered_set<std::string> failed_test_names;
    std::unordered_set<std::string> skipped_test_names;

    bool failed_{false};
};

}

#endif //WLCS_XFAIL_SUPPORTING_TEST_LISTENER_H_
