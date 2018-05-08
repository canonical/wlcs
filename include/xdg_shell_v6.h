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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#ifndef WLCS_XDG_SHELL_V6_
#define WLCS_XDG_SHELL_V6_

#include "in_process_server.h"
#include "generated/xdg-shell-unstable-v6-client.h"

namespace wlcs
{

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

class XdgSurfaceV6: public wlcs::InProcessServer
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
                if (width_ == 1)
                    std::cout << "width is 1" << std::endl;
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
        client->roundtrip();
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

}

#endif // WLCS_XDG_SHELL_V6_
