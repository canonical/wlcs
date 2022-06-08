/*
 * Copyright © 2020 Canonical Ltd.
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

#include "version_specifier.h"
#include "boost/throw_exception.hpp"
#include <stdexcept>

wlcs::ExactlyVersion::ExactlyVersion(uint32_t version) noexcept
    : version{version}
{
}

auto wlcs::ExactlyVersion::select_version(
    uint32_t max_available_version,
    uint32_t max_supported_version) const -> std::optional<uint32_t>
{
    if (version > max_supported_version)
    {
        BOOST_THROW_EXCEPTION(std::logic_error(
            "Required version " +
            std::to_string(version) +
            " is higher than the highest version supported by WLCS (" +
            std::to_string(max_supported_version) +
            ")"));
    }
    else if (version > max_available_version)
    {
        return {};
    }
    else
    {
        return {version};
    }
}

auto wlcs::ExactlyVersion::describe() const -> std::string
{
    return std::string{"= "} + std::to_string(version);
}

wlcs::AtLeastVersion::AtLeastVersion(uint32_t version) noexcept
    : version{version}
{
}

auto wlcs::AtLeastVersion::select_version(
    uint32_t max_available_version,
    uint32_t max_supported_version) const -> std::optional<uint32_t>
{
    if (version > max_supported_version)
    {
        BOOST_THROW_EXCEPTION(std::logic_error(
            "Required version " +
            std::to_string(version) +
            " is higher than the highest version supported by WLCS (" +
            std::to_string(max_supported_version) +
            ")"));
    }
    else if (version > max_available_version)
    {
        return {};
    }
    else
    {
        return {std::min(max_available_version, max_supported_version)};
    }
}

auto wlcs::AtLeastVersion::describe() const -> std::string
{
    return std::string{">= "} + std::to_string(version);
}

wlcs::VersionSpecifier const& wlcs::AnyVersion = wlcs::AtLeastVersion{1};
