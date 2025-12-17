/*
 * Copyright © 2017 Canonical Ltd.
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

#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include <dlfcn.h>

#include "xfail_supporting_test_listener.h"
#include "shared_library.h"
#include "wlcs/display_server.h"

#include "helpers.h"


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    if (argc < 2 || !argv[1] || std::string{"--help"} == argv[1])
    {
        std::cerr
            << "WayLand Conformance Suite test runner" << std::endl
            << "Usage: " << argv[0] << " COMPOSITOR_INTEGRATION_MODULE [GTEST OPTIONS]... [COMPOSITOR_OPTIONS]..." << std::endl;
        return 1;
    }

    auto const integration_filename = argv[1];

    // Shuffle the integration module argument out of argv
    for (auto i = 1 ; i < (argc - 1) ; ++i)
    {
        argv[i] = argv[i + 1];
    }
    wlcs::helpers::set_command_line(argc - 1, const_cast<char const**>(argv));

    std::shared_ptr<wlcs::SharedLibrary> dso;
    try
    {
        dso = std::make_shared<wlcs::SharedLibrary>(integration_filename);
    }
    catch (std::exception const& err)
    {
        std::cerr
            << "Failed to load compositor integration module " << integration_filename << ": " << err.what() << std::endl;
        return 1;
    }

    std::shared_ptr<WlcsServerIntegration const> entry_point;
    try
    {
        entry_point = std::shared_ptr<WlcsServerIntegration const>{
            dso,
            dso->load_function<WlcsServerIntegration const*>("wlcs_server_integration")
        };
    }
    catch (std::exception const& err)
    {
        std::cerr
            << "Failed to load compositor entry point: " << err.what() << std::endl;
        return 1;
    }

    wlcs::helpers::set_entry_point(entry_point);

    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();
    auto wrapping_listener = new testing::XFailSupportingTestListenerWrapper{
        std::unique_ptr<::testing::TestEventListener>{
            listeners.Release(listeners.default_result_printer())}};
    listeners.Append(wrapping_listener);

    /* (void)! is apparently the magical incantation required to get GCC to
     * *actually* silently ignore the return value of a function declared with
     * __attribute__(("warn_unused_result"))
     * cf: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
     */
    (void)!RUN_ALL_TESTS();
    if (wrapping_listener->failed())
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
