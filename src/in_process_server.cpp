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

#include "in_process_server.h"
#include "display_server.h"
#include "helpers.h"
#include "pointer.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <wayland-client.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <experimental/optional>

class ShimNotImplemented : public std::logic_error
{
public:
    ShimNotImplemented() : std::logic_error("Function not implemented in display server shim")
    {
    }
};

class wlcs::Pointer::Impl
{
public:
    Impl(WlcsPointer* raw_device)
        : pointer{raw_device, &wlcs_destroy_pointer}
    {
    }

    void move_to(int x, int y)
    {
        wlcs_pointer_move_absolute(
            pointer.get(),
            wl_fixed_from_int(x),
            wl_fixed_from_int(y));
    }

    void move_by(int dx, int dy)
    {
        wlcs_pointer_move_relative(
            pointer.get(),
            wl_fixed_from_int(dx),
            wl_fixed_from_int(dy));
    }

private:
    std::unique_ptr<WlcsPointer, decltype(&wlcs_destroy_pointer)> const pointer;
};

wlcs::Pointer::~Pointer() = default;
wlcs::Pointer::Pointer(Pointer&&) = default;

wlcs::Pointer::Pointer(WlcsPointer* raw_device)
    : impl{std::make_unique<Impl>(raw_device)}
{
}

void wlcs::Pointer::move_to(int x, int y)
{
    impl->move_to(x, y);
}

void wlcs::Pointer::move_by(int dx, int dy)
{
    impl->move_by(dx, dy);
}

class wlcs::Server::Impl
{
public:
    Impl(int argc, char const** argv)
        : server{wlcs_create_server(argc, argv), &wlcs_destroy_server}
    {
        if (!wlcs_server_start)
        {
            BOOST_THROW_EXCEPTION((std::logic_error{"Missing required wlcs_server_start definition"}));
        }
        if (!wlcs_server_stop)
        {
            BOOST_THROW_EXCEPTION((std::logic_error{"Missing required wlcs_server_stop definition"}));
        }
    }

    void start()
    {
        wlcs_server_start(server.get());
    }

    void stop()
    {
        wlcs_server_stop(server.get());
    }

    int create_client_socket()
    {
        if (wlcs_server_create_client_socket)
        {
            auto fd = wlcs_server_create_client_socket(server.get());
            if (fd < 0)
            {
                BOOST_THROW_EXCEPTION((std::system_error{
                    errno,
                    std::system_category(),
                    "Failed to get client socket from server"}));
            }
            return fd;
        }
        else
        {
            BOOST_THROW_EXCEPTION(ShimNotImplemented{});
        }
    }

    void move_surface_to(Surface& surface, int x, int y)
    {
        // Ensure the server knows about the IDs we're about to send...
        surface.owner().roundtrip();

        wlcs_server_position_window_absolute(server.get(), surface.owner(), surface, x, y);
    }

    WlcsDisplayServer* wlcs_server() const
    {
        return server.get();
    }

private:
    std::unique_ptr<WlcsDisplayServer, void(*)(WlcsDisplayServer*)> const server;
};

wlcs::Server::Server(int argc, char const** argv)
    : impl{std::make_unique<wlcs::Server::Impl>(argc, argv)}
{
}

wlcs::Server::~Server() = default;

void wlcs::Server::start()
{
    impl->start();
}

void wlcs::Server::stop()
{
    impl->stop();
}

int wlcs::Server::create_client_socket()
{
    return impl->create_client_socket();
}

void wlcs::Server::move_surface_to(Surface& surface, int x, int y)
{
    impl->move_surface_to(surface, x, y);
}

wlcs::Pointer wlcs::Server::create_pointer()
{
    if (!wlcs_server_create_pointer || !wlcs_destroy_pointer)
    {
        BOOST_THROW_EXCEPTION((ShimNotImplemented{}));
    }

    return Pointer{wlcs_server_create_pointer(impl->wlcs_server())};
}

wlcs::InProcessServer::InProcessServer()
    : server{helpers::get_argc(), helpers::get_argv()}
{
}

void wlcs::InProcessServer::SetUp()
{
    server.start();
}

void wlcs::InProcessServer::TearDown()
{
    server.stop();
}

wlcs::Server& wlcs::InProcessServer::the_server()
{
    return server;
}

void throw_wayland_error(wl_display* display)
{
    auto err = wl_display_get_error(display);
    if (err != EPROTO)
    {
        BOOST_THROW_EXCEPTION((std::system_error{
            err,
            std::system_category(),
            "Error while dispatching Wayland events"
        }));
    }
    else
    {
        uint32_t object_id;
        uint32_t protocol_error;
        wl_interface const* interface;
        protocol_error = wl_display_get_protocol_error(display, &interface, &object_id);
        BOOST_THROW_EXCEPTION((wlcs::ProtocolError{interface, protocol_error}));
    }
}

class wlcs::Client::Impl
{
public:
    Impl(Server& server)
    {
        try
        {
            display = wl_display_connect_to_fd(server.create_client_socket());
        }
        catch (ShimNotImplemented const&)
        {
            // TODO: Warn about connecting to who-knows-what
            display = wl_display_connect(NULL);
        }

        if (!display)
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to connect to Wayland socket"}));
        }

        registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registry_listener, this);

        server_roundtrip();
    }

    ~Impl()
    {
        // Free any buffers before we destroy the Wayland state…
        client_buffers.clear();

        if (shm) wl_shm_destroy(shm);
        if (shell) wl_shell_destroy(shell);
        if (compositor) wl_compositor_destroy(compositor);
        if (registry) wl_registry_destroy(registry);
        if (shell_surface) wl_shell_surface_destroy(shell_surface);
        if (seat) wl_seat_destroy(seat);
        if (pointer) wl_pointer_destroy(pointer);
        wl_display_disconnect(display);
    }

    struct wl_display* wl_display() const
    {
        return display;
    }

    struct wl_compositor* wl_compositor() const
    {
        return compositor;
    }

    struct wl_shm* wl_shm() const
    {
        return shm;
    }

    Surface create_visible_surface(
        Client& client,
        int width,
        int height)
    {
        Surface surface{client};

        shell_surface = wl_shell_get_shell_surface(shell, surface);
        wl_shell_surface_set_toplevel(shell_surface);

        auto buffer = std::make_shared<ShmBuffer>(client, width, height);

        wl_surface_attach(surface, *buffer, 0, 0);
        wl_surface_commit(surface);

        /*
         * We can't drive buffer cleanup by the buffer.release() event, as that's not
         * guaranteed to be sent in the case of error or server shutdown.
         *
         * Just keep the buffers alive for the lifetime of the client.
         */
        client_buffers.push_back(buffer);

        return surface;
    }

    wl_surface* focused_window() const
    {
        if (current_pointer_location)
        {
            return current_pointer_location->surface;
        }
        return nullptr;
    }

    std::pair<wl_fixed_t, wl_fixed_t> pointer_position() const
    {
        return current_pointer_location.value().coordinates;
    };

    void add_pointer_enter_notification(PointerEnterNotifier const& on_enter)
    {
        enter_notifiers.push_back(on_enter);
    }
    void add_pointer_leave_notification(PointerLeaveNotifier const& on_leave)
    {
        leave_notifiers.push_back(on_leave);
    }

    void dispatch_until(std::function<bool()> const& predicate)
    {
        // TODO: Drive this with epoll on the fd and have a timerfd for timeout
        while (!predicate())
        {
            if (wl_display_dispatch(display) < 0)
            {
                throw_wayland_error(display);
            }
        }
    }

    void server_roundtrip()
    {
        if (wl_display_roundtrip(display) < 0)
        {
            throw_wayland_error(display);
        }
    }

private:
    static void pointer_enter(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t /*serial*/,
        wl_surface* surface,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);

        me->current_pointer_location = PointerLocation {
            surface,
            std::make_pair(x,y)
        };

        std::vector<decltype(enter_notifiers)::const_iterator> to_remove;
        for (auto notifier = me->enter_notifiers.begin(); notifier != me->enter_notifiers.end(); ++notifier)
        {
            if (!(*notifier)(surface, x, y))
            {
                to_remove.push_back(notifier);
            }
        }
        for (auto removed : to_remove)
        {
            me->enter_notifiers.erase(removed);
        }
    }

    static void pointer_leave(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t /*serial*/,
        wl_surface* surface)
    {
        auto me = static_cast<Impl*>(ctx);

        me->current_pointer_location = {};

        std::vector<decltype(leave_notifiers)::const_iterator> to_remove;
        for (auto notifier = me->leave_notifiers.begin(); notifier != me->leave_notifiers.end(); ++notifier)
        {
            if (!(*notifier)(surface))
            {
                to_remove.push_back(notifier);
            }
        }
        for (auto removed : to_remove)
        {
            me->leave_notifiers.erase(removed);
        }
    }

    static void pointer_motion(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t /*time*/,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);

        me->current_pointer_location.value().coordinates = std::make_pair(x, y);
    }

    static constexpr wl_pointer_listener pointer_listener = {
        &Impl::pointer_enter,
        &Impl::pointer_leave,
        &Impl::pointer_motion,
        nullptr,    // button
        nullptr,    // axis
        nullptr,    // frame
        nullptr,    // axis_source
        nullptr,    // axis_stop
        nullptr     // axis_discrete
    };

    static void seat_capabilities(
        void* ctx,
        wl_seat* seat,
        uint32_t capabilities)
    {
        auto me = static_cast<Impl*>(ctx);

        if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        {
            me->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(me->pointer, &pointer_listener, me);
        }
    }

    static void seat_name(
        void*,
        wl_seat*,
        char const*)
    {
    }

    static constexpr wl_seat_listener seat_listener = {
        &Impl::seat_capabilities,
        &Impl::seat_name
    };

    static void global_handler(
        void* ctx,
        wl_registry* registry,
        uint32_t id,
        char const* interface,
        uint32_t version)
    {
        using namespace std::literals::string_literals;

        auto me = static_cast<Impl*>(ctx);

        if ("wl_shm"s == interface)
        {
            me->shm = static_cast<struct wl_shm*>(
                wl_registry_bind(registry, id, &wl_shm_interface, version));
        }
        else if ("wl_compositor"s == interface)
        {
            me->compositor = static_cast<struct wl_compositor*>(
                wl_registry_bind(registry, id, &wl_compositor_interface, version));
        }
        else if ("wl_shell"s == interface)
        {
            me->shell = static_cast<struct wl_shell*>(
                wl_registry_bind(registry, id, &wl_shell_interface, version));
        }
        else if ("wl_seat"s == interface)
        {
            me->seat = static_cast<struct wl_seat*>(
                wl_registry_bind(registry, id, &wl_seat_interface, version));

            wl_seat_add_listener(me->seat, &seat_listener, me);
            // Ensure we receive the initial seat events.
            me->server_roundtrip();
        }
    }

    static void global_removed(void*, wl_registry*, uint32_t)
    {
        // TODO: Remove our globals
    }

    constexpr static wl_registry_listener registry_listener = {
        &global_handler,
        &global_removed
    };

    struct wl_display* display;
    struct wl_registry* registry = nullptr;
    struct wl_compositor* compositor = nullptr;
    struct wl_shm* shm = nullptr;
    struct wl_shell_surface* shell_surface = nullptr;
    struct wl_shell* shell = nullptr;
    struct wl_seat* seat = nullptr;
    struct wl_pointer* pointer = nullptr;

    struct PointerLocation
    {
        wl_surface* surface;
        std::pair<wl_fixed_t, wl_fixed_t> coordinates;
    };
    std::experimental::optional<PointerLocation> current_pointer_location;

    std::vector<std::shared_ptr<ShmBuffer>> client_buffers;
    std::vector<PointerEnterNotifier> enter_notifiers;
    std::vector<PointerLeaveNotifier> leave_notifiers;
};


constexpr wl_pointer_listener wlcs::Client::Impl::pointer_listener;
constexpr wl_seat_listener wlcs::Client::Impl::seat_listener;
constexpr wl_registry_listener wlcs::Client::Impl::registry_listener;

wlcs::Client::Client(Server& server)
    : impl{std::make_unique<Impl>(server)}
{
}

wlcs::Client::~Client() = default;

wlcs::Client::operator wl_display*() const
{
    return impl->wl_display();
}

wl_compositor* wlcs::Client::compositor() const
{
    return impl->wl_compositor();
}

wl_shm* wlcs::Client::shm() const
{
    return impl->wl_shm();
}

wlcs::Surface wlcs::Client::create_visible_surface(int width, int height)
{
    return impl->create_visible_surface(*this, width, height);
}

wl_surface* wlcs::Client::focused_window() const
{
    return impl->focused_window();
}

std::pair<wl_fixed_t, wl_fixed_t> wlcs::Client::pointer_position() const
{
    return impl->pointer_position();
}

void wlcs::Client::add_pointer_enter_notification(PointerEnterNotifier const& on_enter)
{
    impl->add_pointer_enter_notification(on_enter);
}

void wlcs::Client::add_pointer_leave_notification(PointerLeaveNotifier const& on_leave)
{
    impl->add_pointer_leave_notification(on_leave);
}

void wlcs::Client::dispatch_until(std::function<bool()> const& predicate)
{
    impl->dispatch_until(predicate);
}

void wlcs::Client::roundtrip()
{
    impl->server_roundtrip();
}

class wlcs::Surface::Impl
{
public:
    Impl(Client& client)
        : surface_{wl_compositor_create_surface(client.compositor())},
          owner_{client}
    {
    }

    ~Impl()
    {
        for (auto i = 0u; i < pending_callbacks.size(); )
        {
            if (pending_callbacks[i].first == this)
            {
                auto pending_callback = pending_callbacks[i].second;
                delete static_cast<std::function<void(uint32_t)>*>(wl_callback_get_user_data(pending_callback));

                wl_callback_destroy(pending_callback);
                pending_callbacks.erase(pending_callbacks.begin() + i);
            }
            else
            {
                ++i;
            }
        }

        wl_surface_destroy(surface_);
    }

    wl_surface* surface() const
    {
        return surface_;
    }

    void add_frame_callback(std::function<void(uint32_t)> const& on_frame)
    {
        std::unique_ptr<std::function<void(uint32_t)>> holder{
            new std::function<void(uint32_t)>(on_frame)};

        auto callback = wl_surface_frame(surface_);

        pending_callbacks.push_back(std::make_pair(this, callback));

        wl_callback_add_listener(callback, &frame_listener, holder.release());
    }

    Client& owner() const
    {
        return owner_;
    }
private:

    static std::vector<std::pair<Impl const*, wl_callback*>> pending_callbacks;

    static void frame_callback(void* ctx, wl_callback* callback, uint32_t frame_time)
    {
        auto us = std::find_if(
            pending_callbacks.begin(),
            pending_callbacks.end(),
            [callback](auto const& elem)
            {
                return elem.second == callback;
            });
        pending_callbacks.erase(us);

        auto frame_callback = static_cast<std::function<void(uint32_t)>*>(ctx);

        (*frame_callback)(frame_time);

        wl_callback_destroy(callback);
        delete frame_callback;
    }

    static constexpr wl_callback_listener frame_listener = {
        &frame_callback
    };

    struct wl_surface* const surface_;
    Client& owner_;
};

std::vector<std::pair<wlcs::Surface::Impl const*, wl_callback*>> wlcs::Surface::Impl::pending_callbacks;

constexpr wl_callback_listener wlcs::Surface::Impl::frame_listener;

wlcs::Surface::Surface(Client& client)
    : impl{std::make_unique<Impl>(client)}
{
}

wlcs::Surface::~Surface() = default;

wlcs::Surface::Surface(Surface&&) = default;

wlcs::Surface::operator wl_surface*() const
{
    return impl->surface();
}

void wlcs::Surface::add_frame_callback(std::function<void(int)> const& on_frame)
{
    impl->add_frame_callback(on_frame);
}

wlcs::Client& wlcs::Surface::owner() const
{
    return impl->owner();
}

class wlcs::ShmBuffer::Impl
{
public:
    Impl(Client& client, int width, int height)
    {
        auto stride = width * 4;
        auto size = stride * height;
        auto fd = wlcs::helpers::create_anonymous_file(size);

        auto pool = wl_shm_create_pool(client.shm(), fd, size);
        buffer_ = wl_shm_pool_create_buffer(
            pool,
            0,
            width,
            height,
            stride,
            WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        wl_buffer_add_listener(buffer_, &listener, this);
    }

    ~Impl()
    {
        wl_buffer_destroy(buffer_);
    }

    wl_buffer* buffer() const
    {
        return buffer_;
    }

    void add_release_listener(std::function<bool()> const& on_release)
    {
        release_notifiers.push_back(on_release);
    }

private:
    static void on_release(void* ctx, wl_buffer* /*buffer*/)
    {
        auto me = static_cast<Impl*>(ctx);

        std::vector<decltype(me->release_notifiers.begin())> expired_notifiers;

        for (auto notifier = me->release_notifiers.begin(); notifier != me->release_notifiers.end(); ++notifier)
        {
            if (!(*notifier)())
            {
                expired_notifiers.push_back(notifier);
            }
        }
        for (auto const& expired : expired_notifiers)
            me->release_notifiers.erase(expired);
    }

    static constexpr wl_buffer_listener listener {
        &on_release
    };

    wl_buffer* buffer_;
    std::vector<std::function<bool()>> release_notifiers;
};

constexpr wl_buffer_listener wlcs::ShmBuffer::Impl::listener;

wlcs::ShmBuffer::ShmBuffer(Client &client, int width, int height)
    : impl{std::make_unique<Impl>(client, width, height)}
{
}

wlcs::ShmBuffer::ShmBuffer(ShmBuffer&&) = default;
wlcs::ShmBuffer::~ShmBuffer() = default;

wlcs::ShmBuffer::operator wl_buffer*() const
{
    return impl->buffer();
}

void wlcs::ShmBuffer::add_release_listener(std::function<bool()> const &on_release)
{
    impl->add_release_listener(on_release);
}
