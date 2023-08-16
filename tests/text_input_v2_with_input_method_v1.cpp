/*
 * Copyright © Canonical Ltd.
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

#include "in_process_server.h"
#include "mock_text_input_v2.h"
#include "mock_input_method_v1.h"
#include "version_specifier.h"
#include "xdg_shell_stable.h"
#include "method_event_impl.h"

#include <gmock/gmock.h>

// NOTE: In this file, the ordering of app_client.roundtrip() and input_client.roundtrip()
// is important. If we want the text input to respond to an event triggered by the input method,
// we should do:
//    input_client.roundtrip();
//    app_client.roundtrip();
// Inversely, we would swap the order if we want the input method to respond to an event triggered
// by the text input.

using namespace testing;

struct TextInputV2WithInputMethodV1Test : wlcs::StartedInProcessServer
{
    TextInputV2WithInputMethodV1Test()
        : StartedInProcessServer{},
          pointer{the_server().create_pointer()},
          app_client{the_server()},
          input_client{the_server()},
          text_input_manager{app_client.bind_if_supported<zwp_text_input_manager_v2>(wlcs::AnyVersion)},
          text_input{zwp_text_input_manager_v2_get_text_input(text_input_manager, app_client.seat())},
          input_method{input_client.bind_if_supported<zwp_input_method_v1>(wlcs::AnyVersion)}
    {
        zwp_input_method_v1_add_listener(input_method, &listener, this);
    }

    void create_focused_surface()
    {
        app_surface.emplace(app_client.create_visible_surface(100, 100));
        the_server().move_surface_to(app_surface.value(), 0, 0);
        pointer.move_to(10, 10);
        pointer.left_click();
        app_client.roundtrip();
    }

    void activate(zwp_input_method_context_v1* context)
    {
        input_method_context = std::make_unique<NiceMock<wlcs::MockInputMethodContextV1>>(context);
    }

    MOCK_METHOD1(deactivate, void(zwp_input_method_context_v1*));

    static zwp_input_method_v1_listener constexpr listener {
        wlcs::method_event_impl<&TextInputV2WithInputMethodV1Test::activate>,
        wlcs::method_event_impl<&TextInputV2WithInputMethodV1Test::deactivate>
    };

    void enable_text_input()
    {
        create_focused_surface();
        zwp_text_input_v2_enable(text_input, app_surface->wl_surface());
        zwp_text_input_v2_update_state(text_input, 0, 0);
        app_client.roundtrip();
        input_client.roundtrip();
    }

    wlcs::Pointer pointer;
    wlcs::Client app_client;
    wlcs::Client input_client;
    wlcs::WlHandle<zwp_text_input_manager_v2> text_input_manager;
    NiceMock<wlcs::MockTextInputV2> text_input;
    wlcs::WlHandle<zwp_input_method_v1> input_method;
    std::unique_ptr<NiceMock<wlcs::MockInputMethodContextV1>> input_method_context;
    std::optional<wlcs::Surface> app_surface;
};

TEST_F(TextInputV2WithInputMethodV1Test, text_input_enters_surface_on_focus)
{
    wl_surface* entered{nullptr};
    EXPECT_CALL(text_input, enter(_, _))
        .WillOnce(SaveArg<1>(&entered));
    create_focused_surface();
    EXPECT_THAT(entered, Eq(app_surface.value().wl_surface()));
}

TEST_F(TextInputV2WithInputMethodV1Test, text_input_activates_context_on_enable)
{
    create_focused_surface();
    zwp_text_input_v2_enable(text_input, app_surface->wl_surface());
    zwp_text_input_v2_update_state(text_input, 0, 0);
    app_client.roundtrip();
    input_client.roundtrip();
    EXPECT_TRUE(input_method_context != nullptr);
}

TEST_F(TextInputV2WithInputMethodV1Test, text_input_deactivates_context_on_disable)
{
    create_focused_surface();

    EXPECT_CALL(*this, deactivate(_));
    zwp_text_input_v2_enable(text_input, app_surface->wl_surface());
    zwp_text_input_v2_update_state(text_input, 0, 0);
    app_client.roundtrip();
    input_client.roundtrip();
    zwp_text_input_v2_disable(text_input, app_surface->wl_surface());
    zwp_text_input_v2_update_state(text_input, 1, 0);
    app_client.roundtrip();
    input_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, setting_surrounding_text_on_text_input_triggers_a_surround_text_event_on_input_method)
{
    auto const text = "hello";
    auto const cursor = 2;
    auto const anchor = 1;

    enable_text_input();
    EXPECT_CALL(*input_method_context, surrounding_text(text, cursor, anchor));
    zwp_text_input_v2_set_surrounding_text(text_input, text, cursor, anchor);
    zwp_text_input_v2_update_state(text_input, 1, 0);

    app_client.roundtrip();
    input_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_change_text)
{
    auto const text = "hello";

    enable_text_input();
    EXPECT_CALL(text_input, commit_string(text));
    zwp_input_method_context_v1_commit_string(
        *input_method_context, input_method_context->serial, text);

    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_delete_text)
{
    auto const text = "some text";
    int32_t const index = 1;
    int32_t const length = 2;

    enable_text_input();

    EXPECT_CALL(text_input, commit_string(text));
    EXPECT_CALL(text_input, cursor_position(index, 0));
    EXPECT_CALL(text_input, delete_surrounding_text(0, length));
    input_client.roundtrip();
    zwp_input_method_context_v1_delete_surrounding_text(*input_method_context, index, length);
    zwp_input_method_context_v1_commit_string(*input_method_context, input_method_context->serial, text);
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_send_keysym)
{
    uint32_t time = 0;
    uint32_t sym = 65;
    uint32_t state = 1;
    uint32_t modifiers = 0;

    enable_text_input();

    EXPECT_CALL(text_input, keysym(time, sym, state, modifiers));
    zwp_input_method_context_v1_keysym(
        *input_method_context,
        input_method_context->serial,
        time, sym, state, modifiers);
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_set_preedit_string)
{
    auto const preedit_text = "some text";
    auto const preedit_commit = "some fallback text";

    enable_text_input();

    EXPECT_CALL(text_input, preedit_string(preedit_text, preedit_commit));
    zwp_input_method_context_v1_preedit_string(
        *input_method_context,
        input_method_context->serial,
        preedit_text,
        preedit_commit);
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_set_preedit_style)
{
    auto const preedit_text = "some text";
    auto const preedit_commit = "some fallback text";
    auto const index = 0;
    auto const length = 3;
    auto const style = 1;

    enable_text_input();

    EXPECT_CALL(text_input, predit_styling(index, length, style));
    zwp_input_method_context_v1_preedit_styling(
        *input_method_context,
        index,
        length,
        style);
    zwp_input_method_context_v1_preedit_string(
        *input_method_context,
        input_method_context->serial,
        preedit_text,
        preedit_commit);
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_set_preedit_cursor)
{
    auto const preedit_text = "some text";
    auto const preedit_commit = "some fallback text";
    auto const index = 3;

    enable_text_input();

    EXPECT_CALL(text_input, preedit_cursor(index));
    zwp_input_method_context_v1_preedit_cursor(
        *input_method_context,
        index);
    zwp_input_method_context_v1_preedit_string(
        *input_method_context,
        input_method_context->serial,
        preedit_text,
        preedit_commit);
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV2WithInputMethodV1Test, input_method_can_set_modifiers_map)
{
    // Note: This example data was taken from the maliit-keyboard example
    auto const text = "hello";
    const size_t data_length = 33;
    wl_array map;
    wl_array_init(&map);
    const char** data = static_cast<const char**>(wl_array_add(&map, sizeof(char) * data_length));
    *data = "Shift\0Control\0Mod1\0Mod4\0Num Lock\0";

    enable_text_input();

    EXPECT_CALL(text_input, modifiers_map(_));
    zwp_input_method_context_v1_modifiers_map(
        *input_method_context,
        &map);
    zwp_input_method_context_v1_commit_string(*input_method_context, input_method_context->serial, text);
    input_client.roundtrip();
    app_client.roundtrip();
    wl_array_release(&map);
}