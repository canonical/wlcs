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
#include <gmock/gmock.h>

auto wlcs::InputMethod::all_input_methods() -> std::vector<std::shared_ptr<InputMethod>>
{
    return {
        std::make_shared<PointerInputMethod>(),
        std::make_shared<TouchInputMethod>()};
}

struct wlcs::PointerInputMethod::Pointer : Device
{
    Pointer(wlcs::Server& server)
        : pointer{server.create_pointer()}
    {
    }

    void down_at(std::pair<int, int> position) override
    {
        ASSERT_THAT(button_down, testing::Eq(false)) << "Called down_at() with pointer already down";
        pointer.move_to(position.first, position.second);
        pointer.left_button_down();
        button_down = true;
    }

    void move_to(std::pair<int, int> position) override
    {
        ASSERT_THAT(button_down, testing::Eq(true)) << "Called move_to() with pointer up";
        pointer.move_to(position.first, position.second);
    }

    void up() override
    {
        ASSERT_THAT(button_down, testing::Eq(true)) << "Called up() with pointer already up";
        pointer.left_button_up();
        button_down = false;
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

    void down_at(std::pair<int, int> position) override
    {
        ASSERT_THAT(is_down, testing::Eq(false)) << "Called down_at() with touch already down";
        touch.down_at(position.first, position.second);
        is_down = true;
    }

    void move_to(std::pair<int, int> position) override
    {
        ASSERT_THAT(is_down, testing::Eq(true)) << "Called move_to() with touch up";
        touch.move_to(position.first, position.second);
    }

    void up() override
    {
        ASSERT_THAT(is_down, testing::Eq(true)) << "Called up() with touch already up";
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

std::ostream& std::operator<<(std::ostream& out, std::shared_ptr<wlcs::InputMethod> const& param)
{
    return out << param->name;
}
