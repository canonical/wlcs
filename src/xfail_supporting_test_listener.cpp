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
#include "termcolor.hpp"

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
    if (current_skip_reasons)
    {
        for (auto const& reason : *current_skip_reasons)
        {
            std::cout
                << termcolor::yellow << "[          ]"
                << termcolor::reset << " " << reason << std::endl;
        }
        auto elapsed_time = std::chrono::steady_clock::now() - current_test_start;

        std::cout
            << termcolor::yellow << "[     SKIP ]"
            << termcolor::reset << " "
            << test_info.test_case_name() << "." << test_info.name()
            << " ("
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms)" << std::endl;
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

namespace
{
std::string singular_or_plural(char const* base, int count)
{
    if (count == 1)
    {
        return base;
    }
    return std::string{base} + "s";
}
}

void testing::XFailSupportingTestListenerWrapper::OnTestIterationEnd(testing::UnitTest const& unit_test, int /*iteration*/)
{
    std::cout
        << termcolor::green << "[==========] "
        << termcolor::reset
        << unit_test.test_to_run_count()
        << " tests from "
        << unit_test.test_case_to_run_count()
        << " test cases run. ("
        << unit_test.elapsed_time()
        << "ms total elapsed)" << std::endl;

    std::cout
        << termcolor::green << "[  PASSED  ]"
        << termcolor::reset
        << " "
        << unit_test.successful_test_count()
        << singular_or_plural(" test", unit_test.successful_test_count()) << std::endl;

    auto const skipped_tests = skipped_test_names.size();
    if (skipped_tests > 0)
    {
        std::cout
            << termcolor::yellow << "[  SKIPPED ] "
            << termcolor::reset
            << skipped_tests
            << singular_or_plural(" test", skipped_tests)
            << " skipped:" << std::endl;
        for (auto const name : skipped_test_names)
        {
            std::cout
                << termcolor::yellow << "[  SKIPPED ] "
                << termcolor::reset << name << std::endl;
        }
    }
    auto const failed_tests = failed_test_names.size();
    if (failed_tests > 0)
    {
        /* Mark the test run as a failure; if multiple iterations are run
         * only one might fail, making failed_test_names() an unreliable
         * indicator.
         */
        failed_ = true;

        std::cout
            << termcolor::red << "[  FAILED  ] "
            << termcolor::reset
            << failed_tests
            << singular_or_plural(" test", failed_tests)
            << " failed:" << std::endl;
        for (auto const name : failed_test_names)
        {
            std::cout
                << termcolor::red << "[  FAILED  ] "
                << termcolor::reset << name << std::endl;
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
