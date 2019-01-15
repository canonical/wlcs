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

#include "xfail_supporting_test_listener.h"
#include <gtest/gtest.h>
#include <chrono>

namespace testing
{
namespace internal
{
enum class GTestColor
{
    Default, Red, Green, Yellow
};
extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
}
}

testing::XFailSupportingTestListenerWrapper::XFailSupportingTestListenerWrapper(std::unique_ptr<testing::TestEventListener>&& wrapped)
        : delegate{std::move(wrapped)}
{
}

void testing::XFailSupportingTestListenerWrapper::OnTestProgramStart(testing::UnitTest const& unit_test)
{
    delegate->OnTestProgramStart(unit_test);
}

void testing::XFailSupportingTestListenerWrapper::OnTestIterationStart(testing::UnitTest const& unit_test, int iteration)
{
    delegate->OnTestIterationStart(unit_test, iteration);
}

void testing::XFailSupportingTestListenerWrapper::OnEnvironmentsSetUpStart(testing::UnitTest const& unit_test)
{
    delegate->OnEnvironmentsSetUpStart(unit_test);
}

void testing::XFailSupportingTestListenerWrapper::OnEnvironmentsSetUpEnd(testing::UnitTest const& unit_test)
{
    delegate->OnEnvironmentsSetUpEnd(unit_test);
}

void testing::XFailSupportingTestListenerWrapper::OnTestCaseStart(testing::TestCase const& test_case)
{
    delegate->OnTestCaseStart(test_case);
}

void testing::XFailSupportingTestListenerWrapper::OnTestStart(testing::TestInfo const& test_info)
{
    current_test_info = &test_info;
    current_test_start = std::chrono::steady_clock::now();
    delegate->OnTestStart(test_info);
}

void testing::XFailSupportingTestListenerWrapper::OnTestPartResult(testing::TestPartResult const& test_part_result)
{
    if (test_part_result.failed())
    {
        // Search for expected-failure property
        for (int i= 0; i < current_test_info->result()->test_property_count() ; ++i)
        {
            auto prop = current_test_info->result()->GetTestProperty(i);
            if (strcmp(prop.key(), "wlcs-skip-test") == 0)
            {
                if (!current_skip_reasons)
                {
                    current_skip_reasons = std::vector<std::string>{};
                }
                current_skip_reasons->push_back(prop.value());
            }
        }
        if (current_skip_reasons)
        {
            skipped_test_names.insert(std::string{current_test_info->test_case_name()} + "." + current_test_info->name());
            return;
        }

        failed_test_names.insert(std::string{current_test_info->test_case_name()} + "." + current_test_info->name());
    }
    delegate->OnTestPartResult(test_part_result);
}

void testing::XFailSupportingTestListenerWrapper::OnTestEnd(testing::TestInfo const& test_info)
{
    using namespace testing::internal;
    if (current_skip_reasons)
    {
        for (auto const& reason : *current_skip_reasons)
        {
            ColoredPrintf(GTestColor::Yellow, "[          ]");
            ColoredPrintf(GTestColor::Default, " %s\n", reason.c_str());
        }
        auto elapsed_time = std::chrono::steady_clock::now() - current_test_start;

        ColoredPrintf(GTestColor::Yellow, "[     SKIP ]");
        ColoredPrintf(
            GTestColor::Default, " %s.%s (%ld ms)\n",
            test_info.test_case_name(),
            test_info.name(),
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count());
    }
    else
    {
        delegate->OnTestEnd(test_info);
    }
    current_skip_reasons = {};
}

void testing::XFailSupportingTestListenerWrapper::OnTestCaseEnd(testing::TestCase const& test_case)
{
    delegate->OnTestCaseEnd(test_case);
}

void testing::XFailSupportingTestListenerWrapper::OnEnvironmentsTearDownStart(testing::UnitTest const& unit_test)
{
    delegate->OnEnvironmentsTearDownStart(unit_test);
}

void testing::XFailSupportingTestListenerWrapper::OnEnvironmentsTearDownEnd(testing::UnitTest const& unit_test)
{
    delegate->OnEnvironmentsTearDownEnd(unit_test);
}

void testing::XFailSupportingTestListenerWrapper::OnTestIterationEnd(testing::UnitTest const& unit_test, int /*iteration*/)
{
    using namespace testing::internal;
    ColoredPrintf(GTestColor::Green, "[==========] ");

    ColoredPrintf(
        GTestColor::Default, "%i tests from %i test cases run. (%ims total elapsed)\n",
        unit_test.test_to_run_count(),
        unit_test.test_case_to_run_count(),
        unit_test.elapsed_time());
    ColoredPrintf(GTestColor::Green, "[  PASSED  ]");
    ColoredPrintf(GTestColor::Default,
        " %i test%s\n",
        unit_test.successful_test_count(),
        unit_test.successful_test_count() == 1 ? "" : "s");
    if (skipped_test_names.size() > 0)
    {
        ColoredPrintf(GTestColor::Yellow, "[  SKIPPED ]");
        ColoredPrintf(GTestColor::Default,
                      " %i test%s skipped:\n",
                      skipped_test_names.size(),
                      skipped_test_names.size() == 1 ? "" : "s");
        for (auto const name : skipped_test_names)
        {
            ColoredPrintf(GTestColor::Yellow, "[  SKIPPED ]");
            ColoredPrintf(GTestColor::Default, " %s\n", name.c_str());
        }
    }
    if (failed_test_names.size() > 0)
    {
        ColoredPrintf(GTestColor::Red, "[  FAILED  ]");
        ColoredPrintf(GTestColor::Default,
                      " %i test%s failed:\n",
                      failed_test_names.size(),
                      failed_test_names.size() == 1 ? "" : "s");
        for (auto const name : failed_test_names)
        {
            ColoredPrintf(GTestColor::Red, "[  FAILED  ]");
            ColoredPrintf(GTestColor::Default, " %s\n", name.c_str());
        }
    }
}

void testing::XFailSupportingTestListenerWrapper::OnTestProgramEnd(testing::UnitTest const& unit_test)
{
    delegate->OnTestProgramEnd(unit_test);
}

bool testing::XFailSupportingTestListenerWrapper::failed() const
{
    return failed_;
}
