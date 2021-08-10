/*
 * Copyright © 2021 Canonical Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "in_process_server.h"
#include "mock_text_input_v3.h"
#include "mock_input_method_v2.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace testing;

struct TextInputV3WithInputMethodV2Test : wlcs::StartedInProcessServer
{
    TextInputV3WithInputMethodV2Test()
        : StartedInProcessServer{},
          pointer{the_server().create_pointer()},
          app_client{the_server()},
          text_input_manager{app_client.bind_if_supported<zwp_text_input_manager_v3>(wlcs::AnyVersion)},
          text_input{zwp_text_input_manager_v3_get_text_input(text_input_manager, app_client.seat())},
          input_client{the_server()},
          input_method_manager{input_client.bind_if_supported<zwp_input_method_manager_v2>(wlcs::AnyVersion)},
          input_method{zwp_input_method_manager_v2_get_input_method(input_method_manager, input_client.seat())}
    {
    }

    void create_focussed_surface()
    {
        app_surface.emplace(app_client.create_visible_surface(100, 100));
        the_server().move_surface_to(app_surface.value(), 0, 0);
        pointer.move_to(10, 10);
        pointer.left_click();
        app_client.roundtrip();
    }

    wlcs::Pointer pointer;
    wlcs::Client app_client;
    wlcs::WlHandle<zwp_text_input_manager_v3> text_input_manager;
    NiceMock<wlcs::MockTextInputV3> text_input;
    std::optional<wlcs::Surface> app_surface;
    wlcs::Client input_client;
    wlcs::WlHandle<zwp_input_method_manager_v2> input_method_manager;
    NiceMock<wlcs::MockInputMethodV2> input_method;
};

TEST_F(TextInputV3WithInputMethodV2Test, text_input_enters_surface_on_focus)
{
    wl_surface* entered{nullptr};
    EXPECT_CALL(text_input, enter(_))
        .WillOnce(SaveArg<0>(&entered));
    create_focussed_surface();
    EXPECT_THAT(entered, Eq(app_surface.value().operator wl_surface*()));
}

TEST_F(TextInputV3WithInputMethodV2Test, text_input_leaves_surface_on_unfocus)
{
    create_focussed_surface();
    EXPECT_CALL(text_input, enter(_)).Times(0);
    EXPECT_CALL(text_input, leave(app_surface.value().operator wl_surface*()));

    // Create a 2nd client with a focused surface
    wlcs::Client other_client{the_server()};
    auto other_surface = other_client.create_visible_surface(100, 100);
    the_server().move_surface_to(other_surface, 200, 200);
    pointer.move_to(210, 210);
    pointer.left_click();
    app_client.roundtrip();
}

TEST_F(TextInputV3WithInputMethodV2Test, input_method_can_be_enabled)
{
    create_focussed_surface();
    Expectation a = EXPECT_CALL(input_method, activate());
    EXPECT_CALL(input_method, done()).After(a);
    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_commit(text_input);
    app_client.roundtrip();
    input_client.roundtrip();
}

TEST_F(TextInputV3WithInputMethodV2Test, text_field_state_can_be_set)
{
    auto const text = "some text";
    auto const cursor = 2;
    auto const anchor = 1;
    auto const change_cause = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER;
    auto const content_hint =
        ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION |
        ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
    auto const content_purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;

    create_focussed_surface();

    Expectation a = EXPECT_CALL(input_method, activate());
    Expectation b = EXPECT_CALL(input_method, surrounding_text(text, cursor, anchor)).After(a);
    Expectation c = EXPECT_CALL(input_method, text_change_cause(change_cause)).After(a);
    Expectation d = EXPECT_CALL(input_method, content_type(content_hint, content_purpose)).After(a);
    EXPECT_CALL(input_method, done()).After(a, b, c, d);

    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_set_surrounding_text(text_input, text, cursor, anchor);
    zwp_text_input_v3_set_text_change_cause(text_input, change_cause);
    zwp_text_input_v3_set_content_type(text_input, content_hint, content_purpose);
    zwp_text_input_v3_commit(text_input);
    app_client.roundtrip();
    input_client.roundtrip();
}

TEST_F(TextInputV3WithInputMethodV2Test, input_method_can_send_text)
{
    auto const text = "some text";
    auto const delete_left = 1;
    auto const delete_right = 2;

    create_focussed_surface();
    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_commit(text_input);
    app_client.roundtrip();
    input_client.roundtrip();

    Expectation a = EXPECT_CALL(text_input, commit_string(text));
    Expectation b = EXPECT_CALL(text_input, delete_surrounding_text(delete_left, delete_right));
    // Expected serial is 1 because we've sent exactly 1 commit
    EXPECT_CALL(text_input, done(1)).After(a, b);
    zwp_input_method_v2_commit_string(input_method, text);
    zwp_input_method_v2_delete_surrounding_text(input_method, delete_left, delete_right);
    zwp_input_method_v2_commit(input_method, input_method.done_count());
    input_client.roundtrip();
    app_client.roundtrip();
}

TEST_F(TextInputV3WithInputMethodV2Test, input_method_can_send_preedit)
{
    auto const text = "preedit";
    auto const cursor_begin = 1;
    auto const cursor_end = 2;

    create_focussed_surface();
    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_commit(text_input);
    app_client.roundtrip();
    input_client.roundtrip();

    Expectation const a = EXPECT_CALL(text_input, preedit_string(text, cursor_begin, cursor_end));
    // Expected serial is 1 because we've sent exactly 1 commit
    EXPECT_CALL(text_input, done(1)).After(a);
    zwp_input_method_v2_preedit_string(input_method, text, cursor_begin, cursor_end);
    zwp_input_method_v2_commit(input_method, input_method.done_count());
    input_client.roundtrip();
    app_client.roundtrip();
}
