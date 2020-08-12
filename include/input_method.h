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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#ifndef WLCS_INPUT_METHOD_H_
#define WLCS_INPUT_METHOD_H_

#include <string>
#include <vector>
#include "in_process_server.h"

namespace wlcs
{

struct InputMethod
{
    InputMethod(std::string const& name) : name{name} {}
    virtual ~InputMethod() = default;

    struct Device
    {
        virtual ~Device() = default;
        virtual void drag_or_move_to_position(std::pair<int, int> position) = 0;
        virtual void down() = 0; ///< does nothing for touches
        virtual void up() = 0;

        void to_position(std::pair<int, int> position)
        {
            up();
            drag_or_move_to_position(position);
        }
    };

    virtual auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> = 0;
    virtual auto current_surface(wlcs::Client const& client) -> wl_surface* = 0;
    virtual auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> = 0;

    static auto all_input_methods() -> std::vector<std::shared_ptr<InputMethod>>;

    std::string const name;
};

struct PointerInputMethod : InputMethod
{
    PointerInputMethod() : InputMethod{"pointer"} {}

    struct Pointer;

    auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> override;
    auto current_surface(wlcs::Client const& client) -> wl_surface* override;
    auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> override;
};

struct TouchInputMethod : InputMethod
{
    TouchInputMethod() : InputMethod{"touch"} {}

    struct Touch;

    auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> override;
    auto current_surface(wlcs::Client const& client) -> wl_surface* override;
    auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> override;
};

}

std::ostream& operator<<(std::ostream& out, std::shared_ptr<wlcs::InputMethod> const& param);

#endif // WLCS_INPUT_METHOD_H_
