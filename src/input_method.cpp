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

#include "input_method.h"

auto wlcs::InputMethod::all_input_methods() -> std::vector<std::shared_ptr<InputMethod>>
{
    // All this pointer casting nonsense is only to make 16.04 GCC happy
    return {
        std::static_pointer_cast<InputMethod>(std::make_shared<PointerInputMethod>()),
        std::static_pointer_cast<InputMethod>(std::make_shared<TouchInputMethod>())};
}

struct wlcs::PointerInputMethod::Pointer : Device
{
    Pointer(wlcs::Server& server)
        : pointer{server.create_pointer()}
    {
    }

    void drag_or_move_to_position(std::pair<int, int> position) override
    {
        pointer.move_to(position.first, position.second);
    }

    void down() override
    {
        if (!button_down)
        {
            pointer.left_button_down();
            button_down = true;
        }
    }

    void up() override
    {
        if (button_down)
        {
            pointer.left_button_up();
            button_down = false;
        }
    }

    wlcs::Pointer pointer;
    bool button_down = false;
};

auto wlcs::PointerInputMethod::create_device(wlcs::Server& server) -> std::unique_ptr<Device>
{
    return std::make_unique<Pointer>(server);
}

auto wlcs::PointerInputMethod::current_surface(wlcs::Client const& client) -> wl_surface*
{
    return client.window_under_cursor();
}

auto wlcs::PointerInputMethod::position_on_surface(wlcs::Client const& client) -> std::pair<int, int>
{
    auto const wl_fixed_position = client.pointer_position();
    return {
        wl_fixed_to_int(wl_fixed_position.first),
        wl_fixed_to_int(wl_fixed_position.second)};
}

struct wlcs::TouchInputMethod::Touch : Device
{
    Touch(wlcs::Server& server)
        : touch{server.create_touch()}
    {
    }

    void drag_or_move_to_position(std::pair<int, int> position) override
    {
        if (is_down)
        {
            touch.move_to(position.first, position.second);
        }
        else
        {
            touch.down_at(position.first, position.second);
        }
        is_down = true;
    }

    void down() override
    {
    }

    void up() override
    {
        touch.up();
        is_down = false;
    }

    wlcs::Touch touch;
    bool is_down = false;
};

auto wlcs::TouchInputMethod::create_device(wlcs::Server& server) -> std::unique_ptr<Device>
{
    return std::make_unique<Touch>(server);
}

auto wlcs::TouchInputMethod::current_surface(wlcs::Client const& client) -> wl_surface*
{
    return client.touched_window();
}

auto wlcs::TouchInputMethod::position_on_surface(wlcs::Client const& client) -> std::pair<int, int>
{
    auto const wl_fixed_position = client.touch_position();
    return {
        wl_fixed_to_int(wl_fixed_position.first),
        wl_fixed_to_int(wl_fixed_position.second)};
}

std::ostream& operator<<(std::ostream& out, std::shared_ptr<wlcs::InputMethod> const& param)
{
    return out << param->name;
}
