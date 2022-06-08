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

#ifndef WLCS_VERSION_SPECIFIER_H_
#define WLCS_VERSION_SPECIFIER_H_

#include <cstdint>
#include <optional>
#include <string>

namespace wlcs
{
class VersionSpecifier
{
public:
    VersionSpecifier() = default;
    virtual ~VersionSpecifier() = default;

    virtual auto select_version(uint32_t max_version) const -> std::optional<uint32_t> = 0;
    virtual auto describe() const -> std::string = 0;
};

class ExactlyVersion : public VersionSpecifier
{
public:
    explicit ExactlyVersion(uint32_t version) noexcept;

    auto select_version(uint32_t max_version) const -> std::optional<uint32_t> override;
    auto describe() const -> std::string override;
private:
    uint32_t const version;
};

class AtLeastVersion : public VersionSpecifier
{
public:
    explicit AtLeastVersion(uint32_t version) noexcept;

    auto select_version(uint32_t max_version) const -> std::optional<uint32_t> override;
    auto describe() const -> std::string override;
private:
    uint32_t const version;
};

extern VersionSpecifier const& AnyVersion;
}

#endif //WLCS_VERSION_SPECIFIER_H_
