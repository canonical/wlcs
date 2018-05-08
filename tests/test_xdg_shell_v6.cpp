/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Collabora, Ltd.
 * Copyright © 2017 Canonical Ltd.
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

#include "helpers.h"
#include "in_process_server.h"
#include "generated/xdg-shell-unstable-v6-client.h"

#include <gmock/gmock.h>

#include <memory>

class XdgListener
{
public:
    XdgListener(zxdg_surface_v6* shell_surface,
                        zxdg_toplevel_v6* toplevel,
                        std::function<void(uint32_t serial)> surface_configure,
                        std::function<void(int32_t width, int32_t height, struct wl_array *states)> configure, std::function<void()> close)
        : shell_surface{shell_surface},
          toplevel{toplevel},
          surface_configure{surface_configure},
          configure{configure},
          close{close}
    {
        static struct zxdg_surface_v6_listener const surface_listener = {surface_confgure_thunk};
        static struct zxdg_toplevel_v6_listener const toplevel_listener = {configure_thunk, close_thunk};

        zxdg_surface_v6_add_listener(shell_surface, &surface_listener, this);
        zxdg_toplevel_v6_add_listener(toplevel, &toplevel_listener, this);
    }

    zxdg_surface_v6* shell_surface;
    zxdg_toplevel_v6* toplevel;

private:
    std::function<void(uint32_t serial)> surface_configure;
    std::function<void(int32_t width, int32_t height, struct wl_array *states)> configure;
    std::function<void()> close;

    static void surface_confgure_thunk(void *data, struct zxdg_surface_v6 */*surface*/, uint32_t serial)
    {
        static_cast<XdgListener*>(data)->surface_configure(serial);
    }

    static void configure_thunk(void *data, struct zxdg_toplevel_v6 */*toplevel*/, int32_t width, int32_t height, struct wl_array *states)
    {
        static_cast<XdgListener*>(data)->configure(width, height, states);
    }

    static void close_thunk(void *data, struct zxdg_toplevel_v6 */*zxdg_toplevel_v6*/)
    {
        static_cast<XdgListener*>(data)->close();
    }
};

class XdgSurfaceV6Test: public wlcs::InProcessServer
{
public:
    void SetUp() override
    {
        wlcs::InProcessServer::SetUp();
        client = std::make_unique<wlcs::Client>(the_server());
        surface = std::make_unique<wlcs::Surface>(*client);
        shell_surface = zxdg_shell_v6_get_xdg_surface(client->xdg_shell_v6(), *surface);
        toplevel = zxdg_surface_v6_get_toplevel(shell_surface);

        listener = std::make_unique<XdgListener>(
            shell_surface,
            toplevel,
            [this](uint32_t serial){
                // surface configure
                zxdg_surface_v6_ack_configure(this->shell_surface, serial);
                configure_events_count++;
            },
            [this](int32_t width_, int32_t height_, struct wl_array *states)
            {
                // toplevel configure
                window_width = width_;
                window_height = height_;
                window_maximized = false;
                window_fullscreen = false;
                window_resizing = false;
                window_activated = false;
                zxdg_toplevel_v6_state* item;
                for (item = static_cast<zxdg_toplevel_v6_state*>(states->data);
                    item < static_cast<zxdg_toplevel_v6_state*>(states->data) + states->size;
                    item++)
                {
                    switch (*item)
                    {
                        case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
                            window_maximized = true;
                            break;
                        case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
                            window_fullscreen = true;
                            break;
                        case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
                            window_resizing = true;
                            break;
                        case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
                            window_activated = true;
                            break;
                    }
                }
            },
            [this]()
            {
                // toplevel close
                window_should_close = true;
            });

        wl_surface_commit(*surface);
    }

    void TearDown() override
    {
        client->roundtrip();
        buffers.clear();
        zxdg_toplevel_v6_destroy(toplevel);
        zxdg_surface_v6_destroy(shell_surface);
        surface.reset();
        client.reset();
        listener.reset();
        wlcs::InProcessServer::TearDown();
    }

    void attach_buffer(int width, int height)
    {
        buffers.push_back(wlcs::ShmBuffer{*client, width, height});
        wl_surface_attach(*surface, buffers.back(), 0, 0);
        wl_surface_commit(*surface);
    }

    void dispatch_until_configure()
    {
        client->dispatch_until(
            [prev_count = configure_events_count, &current_count = configure_events_count]()
            {
                return current_count > prev_count;
            });
    }

    std::unique_ptr<wlcs::Client> client;
    std::unique_ptr<wlcs::Surface> surface;
    std::vector<wlcs::ShmBuffer> buffers;
    zxdg_surface_v6* shell_surface;
    zxdg_toplevel_v6* toplevel;

    std::unique_ptr<XdgListener> listener;

    int configure_events_count{0};

    bool window_maximized{false};
    bool window_fullscreen{false};
    bool window_resizing{false};
    bool window_activated{false};

    int window_width{-1};
    int window_height{-1};

    bool window_should_close{false};
};

TEST_F(XdgSurfaceV6Test, supports_xdg_shell_v6_protocol)
{
    using namespace testing;

    attach_buffer(600, 400);
    wl_surface_commit(*surface);

    client->roundtrip();
}

TEST_F(XdgSurfaceV6Test, configure_event)
{
    using namespace testing;

    attach_buffer(400, 400);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    attach_buffer(200, 300);
    wl_surface_commit(*surface);

    dispatch_until_configure();
}

TEST_F(XdgSurfaceV6Test, maximize)
{
    using namespace testing;

    attach_buffer(400, 400);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    attach_buffer(200, 300);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    // default values
    EXPECT_EQ(window_width, 0);
    EXPECT_EQ(window_height, 0);
    EXPECT_FALSE(window_maximized);

    //zxdg_surface_v6_set_window_geometry(shell_surface, 0, 0, 200, 300);
    zxdg_toplevel_v6_set_maximized(toplevel);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    EXPECT_TRUE(window_maximized);
    EXPECT_GT(window_width, 0);
    EXPECT_GT(window_height, 0);
}
