/*
 * Copyright © 2017-2019 Canonical Ltd.
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
#include "thread_proxy.h"
#include "version_specifier.h"
#include "wlcs/display_server.h"
#include "helpers.h"
#include "wlcs/pointer.h"
#include "wlcs/touch.h"
#include "xdg_shell_v6.h"
#include "xdg_shell_stable.h"
#include "generated/wayland-client.h"
#include "generated/xdg-shell-unstable-v6-client.h"
#include "generated/xdg-shell-client.h"

#include "linux/input.h"
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <memory>
#include <vector>
#include <algorithm>
#include <optional>
#include <map>
#include <unordered_map>
#include <chrono>
#include <poll.h>

using namespace std::literals::chrono_literals;

class ShimNotImplemented : public std::logic_error
{
public:
    ShimNotImplemented() : std::logic_error("Function not implemented in display server shim")
    {
    }

    ShimNotImplemented(const std::string& name)
        : std::logic_error("Function '" + name + "()' not implemented in display server shim")
    {
    }
};

namespace
{
auto interface_description_if_valid(wl_interface const* interface) -> std::string
{
    if (interface)
    {
        using namespace std::literals::string_literals;
        return interface->name + " v"s + std::to_string(interface->version);
    }
    return "<UNKNOWN INTERFACE>";
}
}
wlcs::ProtocolError::ProtocolError(wl_interface const* interface, uint32_t code)
    : std::system_error(EPROTO, std::system_category(), "Wayland protocol error"),
      interface_{interface},
      code_{code},
      message{
          std::string{"Wayland protocol error: "} +
          std::to_string(code) +
          " on interface " + interface_description_if_valid(interface)}
    {
    }

char const* wlcs::ProtocolError::what() const noexcept
{
    return message.c_str();
}

uint32_t wlcs::ProtocolError::error_code() const
{
    return code_;
}

wl_interface const* wlcs::ProtocolError::interface() const
{
    return interface_;
}

wlcs::ExtensionExpectedlyNotSupported::ExtensionExpectedlyNotSupported(char const* extension, VersionSpecifier const& version)
    : std::runtime_error{
        std::string{"Extension: "} +
        extension + " version " + version.describe() +
        " not supported by compositor under test."}
{
    auto const skip_reason =
        std::string{"Missing extension: "} + extension + version.describe();
    ::testing::Test::RecordProperty("wlcs-skip-test", skip_reason);
}

wlcs::Timeout::Timeout(char const* message)
    : std::runtime_error(message)
{
}

class wlcs::Pointer::Impl
{
public:
    template<typename Proxy>
    Impl(
        WlcsPointer* raw_device,
        std::shared_ptr<Proxy> const& proxy,
        std::shared_ptr<void const> const& keep_dso_loaded)
        : keep_dso_loaded{keep_dso_loaded},
          pointer{raw_device}
    {
        setup_thunks(proxy);
    }

    ~Impl()
    {
        destroy_thunk();
    }

    void move_to(int x, int y)
    {
        move_absolute_thunk(wl_fixed_from_int(x), wl_fixed_from_int(y));
    }

    void move_by(int dx, int dy)
    {
        move_relative_thunk(wl_fixed_from_int(dx), wl_fixed_from_int(dy));
    }

    void button_down(int button)
    {
        button_down_thunk(button);
    }

    void button_up(int button)
    {
        button_up_thunk(button);
    }

private:
    template<typename Proxy>
    void setup_thunks(std::shared_ptr<Proxy> const& proxy)
    {
        move_absolute_thunk = proxy->register_op(
            [this](wl_fixed_t x, wl_fixed_t y)
            {
                pointer->move_absolute(pointer, x, y);
            });
        move_relative_thunk = proxy->register_op(
            [this](wl_fixed_t dx, wl_fixed_t dy)
            {
                pointer->move_relative(pointer, dx, dy);
            });
        button_down_thunk = proxy->register_op(
            [this](int button)
            {
                pointer->button_down(pointer, button);
            });
        button_up_thunk = proxy->register_op(
            [this](int button)
            {
                pointer->button_up(pointer, button);
            });
        destroy_thunk = proxy->register_op(
            [this]()
            {
                pointer->destroy(pointer);
            });
    }

    std::shared_ptr<void const> const keep_dso_loaded;
    WlcsPointer* const pointer;

    std::function<void(wl_fixed_t, wl_fixed_t)> move_absolute_thunk;
    std::function<void(wl_fixed_t, wl_fixed_t)> move_relative_thunk;
    std::function<void(int)> button_down_thunk;
    std::function<void(int)> button_up_thunk;
    std::function<void()> destroy_thunk;
};

wlcs::Pointer::~Pointer() = default;
wlcs::Pointer::Pointer(Pointer&&) = default;

template<typename Proxy>
wlcs::Pointer::Pointer(
    WlcsPointer* raw_device,
    std::shared_ptr<Proxy> const& proxy,
    std::shared_ptr<void const> const& keep_dso_loaded)
    : impl{std::make_unique<Impl>(raw_device, proxy, keep_dso_loaded)}
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

void wlcs::Pointer::button_down(int button)
{
    impl->button_down(button);
}

void wlcs::Pointer::button_up(int button)
{
    impl->button_up(button);
}

void wlcs::Pointer::click(int button)
{
    button_down(button);
    button_up(button);
}

void wlcs::Pointer::left_button_down()
{
    button_down(BTN_LEFT);
}

void wlcs::Pointer::left_button_up()
{
    button_up(BTN_LEFT);
}

void wlcs::Pointer::left_click()
{
    click(BTN_LEFT);
}

class wlcs::Touch::Impl
{
public:
    template<typename Proxy>
    Impl(
        WlcsTouch* raw_device,
        std::shared_ptr<Proxy> const& proxy,
        std::shared_ptr<void const> const& keep_dso_loaded)
        : keep_dso_loaded{keep_dso_loaded},
          touch{raw_device, proxy->register_op([](WlcsTouch* raw_device) { raw_device->destroy(raw_device); })}
    {
        if (touch->version != WLCS_TOUCH_VERSION)
        {
            BOOST_THROW_EXCEPTION((
                std::runtime_error{
                    std::string{"Unexpected WlcsTouch version. Expected: "} +
                    std::to_string(WLCS_TOUCH_VERSION) +
                    " received: " +
                    std::to_string(touch->version)}));
        }
        set_up_thunks(proxy);
    }

    void down_at(int x, int y)
    {
        touch->touch_down(touch.get(), x, y);
    }

    void move_to(int x, int y)
    {
        touch->touch_move(touch.get(), x, y);
    }

    void up()
    {
        touch->touch_up(touch.get());
    }

private:
    template<typename Proxy>
    void set_up_thunks(std::shared_ptr<Proxy> const& proxy)
    {
        touch_down_thunk = proxy->register_op(
            [this](int x, int y)
            {
                touch->touch_down(touch.get(), x, y);
            });
        touch_move_thunk = proxy->register_op(
            [this](int x, int y)
            {
                touch->touch_move(touch.get(), x, y);
            });
        touch_up_thunk = proxy->register_op(
            [this]()
            {
                touch->touch_up(touch.get());
            });
    }

    std::shared_ptr<void const> const keep_dso_loaded;
    std::unique_ptr<WlcsTouch, std::function<void(WlcsTouch*)>> const touch;

    std::function<void(int, int)> touch_down_thunk;
    std::function<void(int, int)> touch_move_thunk;
    std::function<void()> touch_up_thunk;
};

wlcs::Touch::~Touch() = default;
wlcs::Touch::Touch(Touch&&) = default;

template<typename Proxy>
wlcs::Touch::Touch(
    WlcsTouch* raw_device,
    std::shared_ptr<Proxy> const& proxy,
    std::shared_ptr<void const> const& keep_dso_loaded)
    : impl{std::make_unique<Impl>(raw_device, proxy, keep_dso_loaded)}
{
}

void wlcs::Touch::down_at(int x, int y)
{
    impl->down_at(x, y);
}

void wlcs::Touch::move_to(int x, int y)
{
    impl->move_to(x, y);
}

void wlcs::Touch::up()
{
    impl->up();
}

namespace
{
std::shared_ptr<std::unordered_map<std::string, uint32_t> const> extract_supported_extensions(WlcsDisplayServer* server)
{
    if (server->version < 2)
    {
        return {};
    }

    auto const descriptor = server->get_descriptor(server);
    auto extensions = std::make_shared<std::unordered_map<std::string, uint32_t>>();

    for (auto i = 0u; i < descriptor->num_extensions; ++i)
    {
        (*extensions)[descriptor->supported_extensions[i].name] = descriptor->supported_extensions[i].version;
    }
    return extensions;
}

class NullProxy
{
public:
    template<typename Callable>
    auto register_op(Callable handler)
    {
        return handler;
    }
};
}

class wlcs::Server::Impl
{
public:
    Impl(std::shared_ptr<WlcsServerIntegration const> const& hooks, int argc, char const** argv)
        : server{hooks->create_server(argc, argv), hooks->destroy_server},
          thread_context{make_context_if_needed(*server)},
          hooks{hooks},
          supported_extensions_{extract_supported_extensions(server.get())}
    {
        if (hooks->version < 1)
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Server integration too old"}));
        }
        if (server->version < 1)
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Server integration too old"}));
        }
        if (!server->stop)
        {
            BOOST_THROW_EXCEPTION((std::logic_error{"Missing required WlcsDisplayServer.stop definition"}));
        }
        if (thread_context)
        {
            initialise_thunks(thread_context->proxy);
        }
        else
        {
            initialise_thunks(std::make_shared<NullProxy>());
        }
    }

    void start()
    {
        if (thread_context)
        {
            thread_context->server_thread = std::thread{
                [this]()
                {
                    server->start_on_this_thread(server.get(), thread_context->event_loop.get());
                }};
        }
        else
        {
            server->start(server.get());
        }
    }

    void stop()
    {
        stop_thunk();
    }

    int create_client_socket()
    {
        auto fd = create_client_socket_thunk();
        if (fd < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{
                errno,
                std::system_category(),
                "Failed to get client socket from server"}));
        }
        return fd;
    }

    Pointer create_pointer()
    {
        if (thread_context)
        {
            return Pointer{create_pointer_thunk(), thread_context->proxy, hooks};
        }
        else
        {
            return Pointer{create_pointer_thunk(), std::make_shared<NullProxy>(), hooks};
        }
    }

    Touch create_touch()
    {
        if (thread_context)
        {
            return Touch{create_touch_thunk(), thread_context->proxy, hooks};
        }
        else
        {
            return Touch{create_touch_thunk(), std::make_shared<NullProxy>(), hooks};
        }
    }

    void move_surface_to(Surface& surface, int x, int y)
    {
        // Ensure the server knows about the IDs we're about to send...
        surface.owner().roundtrip();

        position_window_absolute_thunk(surface.owner(), surface, x, y);
    }

    WlcsDisplayServer* wlcs_server() const
    {
        return server.get();
    }

    std::shared_ptr<const std::unordered_map<std::string, uint32_t>> supported_extensions() const
    {
        return supported_extensions_;
    }

private:
    struct ThreadContext
    {
        explicit ThreadContext(std::unique_ptr<struct wl_event_loop, decltype(&wl_event_loop_destroy)> loop)
            : event_loop{std::move(loop)},
              proxy{std::make_shared<ThreadProxy>(event_loop.get())}
        {
        }
        ThreadContext(ThreadContext&&) = default;

        ~ThreadContext()
        {
            if (server_thread.joinable())
            {
                server_thread.join();
            }
        }

        std::unique_ptr<struct wl_event_loop, void(*)(struct wl_event_loop*)> event_loop;
        std::thread server_thread;
        std::shared_ptr<ThreadProxy> proxy;
    };

    static std::optional<ThreadContext> make_context_if_needed(WlcsDisplayServer const& server)
    {
        if (server.version >= 3)
        {
            if (!server.start)
            {
                if (!server.start_on_this_thread)
                {
                    BOOST_THROW_EXCEPTION((
                        std::runtime_error{"Server integration missing both start() and start_on_this_thread()"}));
                }
                auto loop = std::unique_ptr<struct wl_event_loop, decltype(&wl_event_loop_destroy)>{
                    wl_event_loop_create(),
                    &wl_event_loop_destroy
                };
                if (!loop)
                {
                    BOOST_THROW_EXCEPTION((
                        std::runtime_error{"Failed to create eventloop for WLCS events"}));
                }

                return {ThreadContext{std::move(loop)}};
            }
        }
        return {};
    };

    std::unique_ptr<WlcsDisplayServer, void(*)(WlcsDisplayServer*)> const server;
    std::optional<ThreadContext> thread_context;
    std::shared_ptr<WlcsServerIntegration const> const hooks;
    std::shared_ptr<std::unordered_map<std::string, uint32_t> const> const supported_extensions_;

    template<typename Proxy>
    void initialise_thunks(std::shared_ptr<Proxy> proxy)
    {
        stop_thunk = proxy->register_op(
            [this]()
            {
                server->stop(server.get());
            });
        create_client_socket_thunk = proxy->register_op(
            [this]()
            {
                return server->create_client_socket(server.get());
            });
        create_pointer_thunk = proxy->register_op(
            [this]()
            {
                return server->create_pointer(server.get());
            });
        create_touch_thunk = proxy->register_op(
            [this]()
            {
                return server->create_touch(server.get());
            });
        position_window_absolute_thunk = proxy->register_op(
            [this](
                struct wl_display* client,
                struct wl_surface* surface,
                int x,
                int y)
            {
                server->position_window_absolute(
                    server.get(),
                    client,
                    surface,
                    x,
                    y);
            });
    }
    std::function<void()> stop_thunk;
    std::function<int()> create_client_socket_thunk;
    std::function<WlcsPointer*()> create_pointer_thunk;
    std::function<WlcsTouch*()> create_touch_thunk;
    std::function<void(struct wl_display*, struct wl_surface*, int, int)> position_window_absolute_thunk;
};

wlcs::Server::Server(
    std::shared_ptr<WlcsServerIntegration const> const& hooks,
    int argc,
    char const** argv)
    : impl{std::make_unique<wlcs::Server::Impl>(hooks, argc, argv)}
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

std::shared_ptr<const std::unordered_map<std::string, uint32_t>> wlcs::Server::supported_extensions()
{
    return impl->supported_extensions();
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
    return impl->create_pointer();
}

wlcs::Touch wlcs::Server::create_touch()
{
    return impl->create_touch();
}

wlcs::InProcessServer::InProcessServer()
    : server{helpers::get_test_hooks(), helpers::get_argc(), helpers::get_argv()}
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
        : supported_extensions{server.supported_extensions()}
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
        if (shm) wl_shm_destroy(shm);
        if (shell) wl_shell_destroy(shell);
        if (compositor) wl_compositor_destroy(compositor);
        if (subcompositor) wl_subcompositor_destroy(subcompositor);
        if (registry) wl_registry_destroy(registry);
        if (seat) wl_seat_destroy(seat);
        if (keyboard) wl_keyboard_destroy(keyboard);
        if (pointer) wl_pointer_destroy(pointer);
        if (touch) wl_touch_destroy(touch);
        if (xdg_shell_v6) zxdg_shell_v6_destroy(xdg_shell_v6);
        if (xdg_shell_stable) xdg_wm_base_destroy(xdg_shell_stable);
        for (auto const& output: outputs)
            wl_output_destroy(output->current.output);
        for (auto const& callback: destruction_callbacks)
            callback();
        destruction_callbacks.clear();
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

    struct wl_subcompositor* wl_subcompositor() const
    {
        return subcompositor;
    }

    struct wl_shm* wl_shm() const
    {
        return shm;
    }

    struct wl_seat* wl_seat() const
    {
        return seat;
    }

    void run_on_destruction(std::function<void()> callback)
    {
        destruction_callbacks.push_back(callback);
    }

    ShmBuffer const& create_buffer(Client& client, int width, int height)
    {
        auto buffer = std::make_shared<ShmBuffer>(client, width, height);
        run_on_destruction([buffer]() mutable
            {
                buffer.reset();
            });
        return *buffer;
    }

    Surface create_wl_shell_surface(Client& client, int width, int height)
    {
        Surface surface{client};

        wl_shell_surface * shell_surface = wl_shell_get_shell_surface(the_shell(), surface);
        surface.run_on_destruction([shell_surface]()
            {
                wl_shell_surface_destroy(shell_surface);
            });
        wl_shell_surface_set_toplevel(shell_surface);

        wl_surface_commit(surface);

        surface.attach_visible_buffer(width, height);

        return surface;
    }

    Surface create_xdg_shell_v6_surface(Client& client, int width, int height)
    {
        Surface surface{client};

        auto xdg_surface = std::make_shared<XdgSurfaceV6>(client, surface);
        auto xdg_toplevel = std::make_shared<XdgToplevelV6>(*xdg_surface);

        surface.run_on_destruction([xdg_surface, xdg_toplevel]() mutable
            {
                xdg_toplevel.reset();
                xdg_surface.reset();
            });

        wl_surface_commit(surface);

        surface.attach_visible_buffer(width, height);

        return surface;
    }

    Surface create_xdg_shell_stable_surface(Client& client, int width, int height)
    {
        Surface surface{client};

        auto xdg_surface = std::make_shared<XdgSurfaceStable>(client, surface);
        auto xdg_toplevel = std::make_shared<XdgToplevelStable>(*xdg_surface);

        // Protocol *requires* us to ack the initial configure before attaching a buffer
        bool initial_configure_received = false;
        EXPECT_CALL(*xdg_surface, configure(testing::_))
            .WillOnce(
                [&initial_configure_received, xdg_surface = static_cast<struct xdg_surface*>(*xdg_surface)](uint32_t serial)
                {
                    xdg_surface_ack_configure(xdg_surface, serial);
                    initial_configure_received = true;
                });

        surface.run_on_destruction([xdg_surface, xdg_toplevel]() mutable
            {
                xdg_toplevel.reset();
                xdg_surface.reset();
            });

        wl_surface_commit(surface);

        client.dispatch_until([&initial_configure_received]() { return initial_configure_received; });
        /* Make absolutely sure the functor above, which captures a stack variable by reference
         * is not going to be invoked outside this function
         */
        testing::Mock::VerifyAndClearExpectations(xdg_surface.get());

        /* And we might as well ack any subsequent configures.
         * Since we don't need to capture anything but the xdg_surface, which has to be
         * live if we're getting a callback on it, this is safe to escape this frame
         */
        ON_CALL(*xdg_surface, configure(testing::_))
            .WillByDefault(
                [xdg_surface = static_cast<struct xdg_surface*>(*xdg_surface)](uint32_t serial)
                {
                    xdg_surface_ack_configure(xdg_surface, serial);
                });

        surface.attach_visible_buffer(width, height);

        return surface;
    }

    Surface create_visible_surface(Client& client, int width, int height)
    {
        if (shell)
        {
            return create_wl_shell_surface(client, width, height);
        }
        else if (xdg_shell_stable)
        {
            return create_xdg_shell_stable_surface(client, width, height);
        }
        else if (xdg_shell_v6)
        {
            return create_xdg_shell_v6_surface(client, width, height);
        }
        else
        {
            throw std::runtime_error("compositor does not support any known shell protocols");
        }
    }

    wl_shell* the_shell() const
    {
        if (shell)
        {
            return shell;
        }
        else
        {
            if (!supported_extensions || !supported_extensions->count("wl_shell"))
            {
                BOOST_THROW_EXCEPTION((ExtensionExpectedlyNotSupported{"wl_shell", AnyVersion}));
            }
            else
            {
                throw std::runtime_error("Failed to bind to wl_shell");
            }
        }
    }

    zxdg_shell_v6* the_xdg_shell_v6() const
    {
        if (xdg_shell_v6)
        {
            return xdg_shell_v6;
        }
        else
        {
            if (!supported_extensions || !supported_extensions->count("zxdg_shell_v6"))
            {
                BOOST_THROW_EXCEPTION((ExtensionExpectedlyNotSupported{"zxdg_shell_v6", AnyVersion}));
            }
            else
            {
                throw std::runtime_error("Failed to bind to zxdg_shell_v6");
            }
        }
    }

    xdg_wm_base* the_xdg_shell_stable() const
    {
        return xdg_shell_stable;
    }

    wl_pointer* the_pointer() const { return pointer; }

    wl_surface* keyboard_focused_window() const
    {
        return keyboard_focused_surface;
    }

    wl_surface* window_under_cursor() const
    {
        if (current_pointer_location)
        {
            return current_pointer_location->surface;
        }
        return nullptr;
    }

    wl_surface* touched_window() const
    {
        wl_surface* surface = nullptr;
        for (auto const& touch : current_touches)
        {
            if (surface && touch.second.surface != surface)
                BOOST_THROW_EXCEPTION(std::runtime_error("Multiple surfaces have active touches"));
            else
                surface = touch.second.surface;
        }
        return surface;
    }

    std::pair<wl_fixed_t, wl_fixed_t> pointer_position() const
    {
        return current_pointer_location.value().coordinates;
    };

    std::pair<wl_fixed_t, wl_fixed_t> touch_position() const
    {
        if (current_touches.empty())
            BOOST_THROW_EXCEPTION(std::runtime_error("No touches"));
        else if (current_touches.size() == 1)
            return current_touches.begin()->second.coordinates;
        else
            BOOST_THROW_EXCEPTION(std::runtime_error("More than one touches"));
    };

    std::optional<uint32_t> latest_serial() const
    {
        return latest_serial_;
    }

    bool pointer_events_pending() const
    {
        return !pending_buttons.empty() || pending_pointer_location;
    }

    bool touch_events_pending() const
    {
        return !pending_touches.empty() || !pending_up_touches.empty();
    }

    void add_pointer_enter_notification(PointerEnterNotifier const& on_enter)
    {
        enter_notifiers.push_back(on_enter);
    }
    void add_pointer_leave_notification(PointerLeaveNotifier const& on_leave)
    {
        leave_notifiers.push_back(on_leave);
    }
    void add_pointer_motion_notification(PointerMotionNotifier const& on_motion)
    {
        motion_notifiers.push_back(on_motion);
    }
    void add_pointer_button_notification(PointerButtonNotifier const& on_button)
    {
        button_notifiers.push_back(on_button);
    }

    void* bind_if_supported(wl_interface const& to_bind, VersionSpecifier const& version) const
    {
        std::optional<bool> expected_to_be_supported{};

        if (supported_extensions)
        {
            expected_to_be_supported =
                supported_extensions->count(to_bind.name) &&
                version.select_version(supported_extensions->at(to_bind.name), to_bind.version);
        }

        /* TODO: Mark tests using globals which exist, but are listed as unsupported as
         * may-fail.
         * This should help incrementally implementing a protocol, while getting test
         * feedback.
         */
        auto const global = globals.find(to_bind.name);
        if (global != globals.end())
        {
            auto const version_to_bind = version.select_version(global->second.version, to_bind.version);
            if (version_to_bind)
            {
                auto global_proxy = wl_registry_bind(registry, global->second.id, &to_bind, version_to_bind.value());
                if (!global_proxy)
                {
                    throw_wayland_error(display);
                }
                return global_proxy;
            }
            else if (!expected_to_be_supported.value_or(true))
            {
                // We didn't expect to find the needed version of this extension anyway…
                BOOST_THROW_EXCEPTION((ExtensionExpectedlyNotSupported{to_bind.name, version}));
            }

            BOOST_THROW_EXCEPTION((std::runtime_error{
                "Failed to bind to " + std::string{to_bind.name} + " version " + version.describe()}));
        }
        else if (!expected_to_be_supported.value_or(true))
        {
            // We didn't expect to find this extension anyway…
            BOOST_THROW_EXCEPTION((ExtensionExpectedlyNotSupported{to_bind.name, version}));
        }
        BOOST_THROW_EXCEPTION((std::runtime_error{
            "Failed to bind to " + std::string{to_bind.name} + " version " + version.describe()}));
    }

    void dispatch_until(
        std::function<bool()> const& predicate, std::chrono::seconds timeout)
    {
        using namespace std::chrono;

        auto const end_time = steady_clock::now() + timeout;
        while (!predicate())
        {
            while (wl_display_prepare_read(display) != 0)
            {
                if (wl_display_dispatch_pending(display) < 0)
                    throw_wayland_error(display);
            }
            wl_display_flush(display);

            auto const time_left = end_time - steady_clock::now();
            if (time_left.count() < 0)
            {
                BOOST_THROW_EXCEPTION((Timeout{"Timeout waiting for condition"}));
            }

            /*
             * TODO: We really want std::chrono::duration::ceil() here, but that's C++17
             */
            /*
             * We want to wait *at least* as long as time_left. duration_cast<milliseconds>
             * will perform integer division, so any fractional milliseconds will get dropped.
             *
             * Adding 1ms will ensure we wait until *after* we're meant to timeout.
             */
            auto const maximum_wait_ms = duration_cast<milliseconds>(time_left) + 1ms;
            pollfd fd{
                wl_display_get_fd(display),
                POLLIN | POLLERR,
                0
            };

            auto const poll_result = poll(&fd, 1, maximum_wait_ms.count());
            if (poll_result < 0)
            {
                wl_display_cancel_read(display);
                BOOST_THROW_EXCEPTION((std::system_error{
                    errno,
                    std::system_category(),
                    "Failed to wait for Wayland event"}));
            }

            if (poll_result == 0)
            {
                wl_display_cancel_read(display);
                BOOST_THROW_EXCEPTION((Timeout{"Timeout waiting for condition"}));
            }

            if (wl_display_read_events(display) < 0)
            {
                throw_wayland_error(display);
            }

            if (wl_display_dispatch_pending(display) < 0)
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

    void client_flush()
    {
        if (wl_display_flush(display) == -1)
        {
            /* flush will return a (non-fatal) EAGAIN if the send buffer is
             * full.
             *
             * We don't particularly want to care about the EAGAIN case
             * at the moment, so just ignore it.
             */
            if (errno != EAGAIN)
            {
                throw_wayland_error(display);
            }
        }
    }

    struct Output
    {
        OutputState current;
        OutputState pending;
        std::vector<std::function<void()>> done_notifiers;

        Output(struct wl_output* output)
            : current{output},
              pending{output}
        {
        }

        static void geometry_thunk(
            void* ctx,
            struct wl_output */*wl_output*/,
            int32_t x,
            int32_t y,
            int32_t /*physical_width*/,
            int32_t /*physical_height*/,
            int32_t /*subpixel*/,
            const char */*make*/,
            const char */*model*/,
            int32_t /*transform*/)
        {
            auto me = static_cast<Output*>(ctx);
            me->pending.geometry_position = std::make_pair(x, y);
        }

        static void mode_thunk(
            void* ctx,
            struct wl_output */*wl_output*/,
            uint32_t /*flags*/,
            int32_t width,
            int32_t height,
            int32_t /*refresh*/)
        {
            auto me = static_cast<Output*>(ctx);
            me->pending.mode_size = std::make_pair(width, height);
        }

        static void done_thunk(void* ctx, struct wl_output */*wl_output*/)
        {
            auto me = static_cast<Output*>(ctx);

            if (me->pending.geometry_position)
                me->current.geometry_position = me->pending.geometry_position;
            if (me->pending.mode_size)
                me->current.mode_size = me->pending.mode_size;
            if (me->pending.scale)
                me->current.scale = me->pending.scale;

            me->pending = OutputState{me->current.output};

            for (auto const& notifier : me->done_notifiers)
                notifier();
        }

        static void scale_thunk(void* ctx, struct wl_output */*wl_output*/, int32_t factor)
        {
            auto me = static_cast<Output*>(ctx);
            me->pending.scale = factor;
        }

        static constexpr wl_output_listener listener = {
            &Impl::Output::geometry_thunk,
            &Impl::Output::mode_thunk,
            &Impl::Output::done_thunk,
            &Impl::Output::scale_thunk,
        };
    };

    std::vector<std::unique_ptr<Output>> outputs;

private:
    static void keyboard_keymap(void*, wl_keyboard*, uint32_t, int32_t fd, uint32_t)
    {
        close(fd);
    }

    static void keyboard_enter(
        void* ctx,
        wl_keyboard*,
        uint32_t serial,
        wl_surface *surface,
        wl_array* /*keys*/)
    {
        auto me = static_cast<Impl*>(ctx);
        me->keyboard_focused_surface = surface;
        me->latest_serial_ = serial;
    }

    static void keyboard_leave(
        void *ctx,
        wl_keyboard*,
        uint32_t serial,
        wl_surface* surface)
    {
        auto me = static_cast<Impl*>(ctx);
        if (me->keyboard_focused_surface == surface)
        {
            me->keyboard_focused_surface = nullptr;
        }
        me->latest_serial_ = serial;
    }

    static void keyboard_key(
        void* ctx,
        wl_keyboard*,
        uint32_t serial,
        uint32_t /*time*/,
        uint32_t /*key*/,
        uint32_t /*state*/)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;
    }

    static void keyboard_modifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
    {
    }

    static void keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t)
    {
    }

    static constexpr wl_keyboard_listener keyboard_listener = {
        keyboard_keymap,
        keyboard_enter,
        keyboard_leave,
        keyboard_key,
        keyboard_modifiers,
        keyboard_repeat_info,
    };

    static void pointer_enter(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t serial,
        wl_surface* surface,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;

        if (me->current_pointer_location && !me->pending_pointer_leave)
            FAIL()
                << "Pointer tried to enter surface " << surface
                << " without first leaving surface " << me->current_pointer_location.value().surface;

        me->pending_pointer_location = SurfaceLocation{
            surface,
            std::make_pair(x, y)
        };
    }

    static void pointer_leave(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t serial,
        wl_surface* surface)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;

        if (!me->current_pointer_location)
            FAIL() << "Got wl_pointer.leave when the pointer was not on a surface";

        // the surface should never be null along the wire, but may come out as null if it's been destroyed
        if (surface != nullptr && surface != me->current_pointer_location.value().surface)
            FAIL()
                << "Got wl_pointer.leave with surface " << surface
                << " instead of " << me->current_pointer_location.value().surface;

        me->pending_pointer_location = std::nullopt;
        me->pending_pointer_leave = true;
    }

    static void pointer_motion(
        void* ctx,
        wl_pointer* /*pointer*/,
        uint32_t /*time*/,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);

        if (!me->current_pointer_location && !me->pending_pointer_location)
            FAIL() << "Got wl_pointer.motion when the pointer was not on a surface";

        if (!me->pending_pointer_location)
            me->pending_pointer_location = me->current_pointer_location;
        me->pending_pointer_location.value().coordinates = std::make_pair(x, y);
    }

    static void pointer_button(
        void *ctx,
        wl_pointer* /*wl_pointer*/,
        uint32_t serial,
        uint32_t /*time*/,
        uint32_t button,
        uint32_t state)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;

        me->pending_buttons[button] = std::make_pair(serial, state == WL_POINTER_BUTTON_STATE_PRESSED);
    }

    static void pointer_frame(void* ctx, wl_pointer* /*pointer*/)
    {
        auto me = static_cast<Impl*>(ctx);

        if (me->pending_pointer_leave)
        {
            if (!me->current_pointer_location)
                FAIL() << "Pointer tried to leave when it was not on a surface";

            wl_surface* old_surface = me->current_pointer_location.value().surface;
            me->current_pointer_location = std::nullopt;
            me->pending_pointer_leave = false;

            me->notify_of_pointer_leave(old_surface);
        }

        if (me->pending_pointer_location)
        {
            auto const old_pointer_location = me->current_pointer_location;
            me->current_pointer_location = me->pending_pointer_location;
            me->pending_pointer_location = std::nullopt;

            if (!old_pointer_location)
            {
                me->notify_of_pointer_enter(
                    me->current_pointer_location.value().surface,
                    me->current_pointer_location.value().coordinates);
            }
            else
            {
                me->notify_of_pointer_motion(me->current_pointer_location.value().coordinates);
            }
        }

        if (!me->pending_buttons.empty())
        {
            me->notify_of_pointer_buttons(me->pending_buttons);
            me->pending_buttons.clear();
        }
    }

    void notify_of_pointer_enter(wl_surface* surface, std::pair<int, int> position)
    {
        std::vector<decltype(enter_notifiers)::const_iterator> to_remove;
        for (auto notifier = enter_notifiers.begin(); notifier != enter_notifiers.end(); ++notifier)
        {
            if (!(*notifier)(surface, position.first, position.second))
            {
                to_remove.push_back(notifier);
            }
        }
        for (auto removed : to_remove)
        {
            enter_notifiers.erase(removed);
        }
    }

    void notify_of_pointer_leave(wl_surface* surface)
    {
        std::vector<decltype(leave_notifiers)::const_iterator> to_remove;
        for (auto notifier = leave_notifiers.begin(); notifier != leave_notifiers.end(); ++notifier)
        {
            if (!(*notifier)(surface))
            {
                to_remove.push_back(notifier);
            }
        }
        for (auto removed : to_remove)
        {
            leave_notifiers.erase(removed);
        }
    }

    void notify_of_pointer_motion(std::pair<int, int> position)
    {

        std::vector<decltype(motion_notifiers)::const_iterator> to_remove;
        for (auto notifier = motion_notifiers.begin(); notifier != motion_notifiers.end(); ++notifier)
        {
            if (!(*notifier)(position.first, position.second))
            {
                to_remove.push_back(notifier);
            }
        }
        for (auto removed : to_remove)
        {
            motion_notifiers.erase(removed);
        }
    }

    void notify_of_pointer_buttons(std::map<uint32_t, std::pair<uint32_t, bool>> const& buttons)
    {
        for (auto const& button : buttons)
        {
            std::vector<decltype(button_notifiers)::const_iterator> to_remove;
            for (auto notifier = button_notifiers.begin(); notifier != button_notifiers.end(); ++notifier)
            {
                if (!(*notifier)(button.second.first, button.first, button.second.second))
                {
                    to_remove.push_back(notifier);
                }
            }
            for (auto removed : to_remove)
            {
                button_notifiers.erase(removed);
            }
        }
    }

    static constexpr wl_pointer_listener pointer_listener = {
        &Impl::pointer_enter,
        &Impl::pointer_leave,
        &Impl::pointer_motion,
        &Impl::pointer_button,
        [](auto...){},  // axis
        &Impl::pointer_frame,    // frame
        [](auto...){},  // axis_source
        [](auto...){},  // axis_stop
        [](auto...){},  // axis_discrete
    };

    static void touch_down(
        void* ctx,
        wl_touch* /*wl_touch*/,
        uint32_t serial,
        uint32_t /*time*/,
        wl_surface* surface,
        int32_t id,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;

        auto touch = me->current_touches.find(id);
        if (touch != me->current_touches.end())
            FAIL() << "Got wl_touch.down with ID " << id << " which is already down";

        me->pending_touches[id] = SurfaceLocation {
            surface,
            std::make_pair(x,y)
        };
    }

    static void touch_up(
        void* ctx,
        wl_touch* /*wl_touch*/,
        uint32_t serial,
        uint32_t /*time*/,
        int32_t id)
    {
        auto me = static_cast<Impl*>(ctx);
        me->latest_serial_ = serial;

        auto touch = me->current_touches.find(id);
        if (touch == me->current_touches.end())
            FAIL() << "Got wl_touch.up with unknown ID " << id;

        me->pending_up_touches.insert(id);
    }

	static void touch_motion(
        void* ctx,
        wl_touch* /*wl_touch*/,
        uint32_t /*time*/,
        int32_t id,
        wl_fixed_t x,
        wl_fixed_t y)
    {
        auto me = static_cast<Impl*>(ctx);

        auto touch = me->current_touches.find(id);
        if (touch == me->current_touches.end())
            FAIL() << "Got wl_touch.up with unknown ID " << id;

        me->pending_touches[id] = SurfaceLocation {
            touch->second.surface,
            std::make_pair(x,y)
        };
    }

    static void touch_frame(void* ctx, wl_touch* /*touch*/)
    {
        auto me = static_cast<Impl*>(ctx);

        for (auto const& id : me->pending_up_touches)
        {
            me->current_touches.erase(id);
        }

        for (auto const& touch : me->pending_touches)
        {
            me->current_touches[touch.first] = touch.second;
        }

        me->pending_up_touches.clear();
        me->pending_touches.clear();
    }

    static constexpr wl_touch_listener touch_listener = {
        &Impl::touch_down,
        &Impl::touch_up,
        &Impl::touch_motion,
        &Impl::touch_frame,
        nullptr,    // cancel
        nullptr,    // shape
        nullptr,    // orientation
    };

    static void seat_capabilities(
        void* ctx,
        struct wl_seat* seat,
        uint32_t capabilities)
    {
        auto me = static_cast<Impl*>(ctx);

        if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        {
            me->keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(me->keyboard, &keyboard_listener, me);
        }

        if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        {
            me->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(me->pointer, &pointer_listener, me);
        }

        if (capabilities & WL_SEAT_CAPABILITY_TOUCH)
        {
            me->touch = wl_seat_get_touch(seat);
            wl_touch_add_listener(me->touch, &touch_listener, me);
        }
    }

    static void seat_name(
        void*,
        struct wl_seat*,
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

        static auto const safe_bind = []
            (wl_registry* registry, uint32_t name, const wl_interface* iface, uint32_t version)
        {
            return wl_registry_bind(registry, name, iface, std::min(version, static_cast<uint32_t>(iface->version)));
        };

        auto me = static_cast<Impl*>(ctx);
        me->global_type_names[id] = interface;
        me->globals[interface] = Global{id, version};

        if ("wl_shm"s == interface)
        {
            me->shm = static_cast<struct wl_shm*>(safe_bind(registry, id, &wl_shm_interface, version));
        }
        else if ("wl_compositor"s == interface)
        {
            me->compositor = static_cast<struct wl_compositor*>(
                safe_bind(registry, id, &wl_compositor_interface, version));
        }
        else if ("wl_subcompositor"s == interface)
        {
            me->subcompositor = static_cast<struct wl_subcompositor*>(
                safe_bind(registry, id, &wl_subcompositor_interface, version));
        }
        else if ("wl_shell"s == interface)
        {
            me->shell = static_cast<wl_shell*>(safe_bind(registry, id, &wl_shell_interface, version));
        }
        else if ("wl_seat"s == interface)
        {
            me->seat = static_cast<struct wl_seat*>(safe_bind(registry, id, &wl_seat_interface, version));

            wl_seat_add_listener(me->seat, &seat_listener, me);
            // Ensure we receive the initial seat events.
            me->server_roundtrip();
        }
        else if ("wl_output"s == interface)
        {
            auto wl_output = static_cast<struct wl_output*>(safe_bind(registry, id, &wl_output_interface, version));
            auto output = std::make_unique<Output>(wl_output);
            wl_output_add_listener(wl_output, &Output::listener, output.get());
            me->outputs.push_back(std::move(output));

            // Ensure we receive the initial output events.
            me->server_roundtrip();
        }
        else if ("zxdg_shell_v6"s == interface)
        {
            me->xdg_shell_v6 = static_cast<zxdg_shell_v6*>(safe_bind(registry, id, &zxdg_shell_v6_interface, version));
        }
        else if ("xdg_wm_base"s == interface)
        {
            me->xdg_shell_stable = static_cast<xdg_wm_base*>(safe_bind(registry, id, &xdg_wm_base_interface, version));
        }
    }

    static void global_removed(void* ctx, wl_registry*, uint32_t id)
    {
        auto me = static_cast<Impl*>(ctx);
        auto const name = me->global_type_names.find(id);
        if (name != me->global_type_names.end())
        {
            me->globals.erase(name->second);
            me->global_type_names.erase(name);
        }
    }

    constexpr static wl_registry_listener registry_listener = {
        &global_handler,
        &global_removed
    };

    std::shared_ptr<std::unordered_map<std::string, uint32_t> const> const supported_extensions;

    struct wl_display* display;
    struct wl_registry* registry = nullptr;
    struct wl_compositor* compositor = nullptr;
    struct wl_subcompositor* subcompositor = nullptr;
    struct wl_shm* shm = nullptr;
    struct wl_shell* shell = nullptr;
    struct wl_seat* seat = nullptr;
    struct wl_keyboard* keyboard = nullptr;
    struct wl_pointer* pointer = nullptr;
    struct wl_touch* touch = nullptr;
    struct zxdg_shell_v6* xdg_shell_v6 = nullptr;
    std::vector<std::function<void()>> destruction_callbacks;
    struct xdg_wm_base* xdg_shell_stable = nullptr;

    struct Global
    {
        uint32_t id;
        uint32_t version;
    };
    std::map<std::string, Global> globals;
    std::map<uint32_t, std::string> global_type_names;

    struct SurfaceLocation
    {
        wl_surface* surface;
        std::pair<wl_fixed_t, wl_fixed_t> coordinates;
    };
    wl_surface* keyboard_focused_surface = nullptr;
    std::optional<SurfaceLocation> current_pointer_location;
    std::optional<SurfaceLocation> pending_pointer_location;
    bool pending_pointer_leave{false};
    std::map<uint32_t, std::pair<uint32_t, bool>> pending_buttons; ///< Maps button id to the serial and if it's down
    std::map<int, SurfaceLocation> current_touches; ///< Touches that have gotten a frame event
    std::map<int, SurfaceLocation> pending_touches; ///< Touches that have gotten down or motion events without a frame
    std::set<int> pending_up_touches; ///< Touches that have gotten up events without a frame
    std::optional<uint32_t> latest_serial_;

    std::vector<PointerEnterNotifier> enter_notifiers;
    std::vector<PointerLeaveNotifier> leave_notifiers;
    std::vector<PointerMotionNotifier> motion_notifiers;
    std::vector<PointerButtonNotifier> button_notifiers;
};

constexpr wl_keyboard_listener wlcs::Client::Impl::keyboard_listener;
constexpr wl_pointer_listener wlcs::Client::Impl::pointer_listener;
constexpr wl_touch_listener wlcs::Client::Impl::touch_listener;
constexpr wl_seat_listener wlcs::Client::Impl::seat_listener;
constexpr wl_output_listener wlcs::Client::Impl::Output::listener;
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

wl_subcompositor* wlcs::Client::subcompositor() const
{
    return impl->wl_subcompositor();
}

wl_shm* wlcs::Client::shm() const
{
    return impl->wl_shm();
}

wl_seat* wlcs::Client::seat() const
{
    return impl->wl_seat();
}

void wlcs::Client::run_on_destruction(std::function<void()> callback)
{
    impl->run_on_destruction(callback);
}

wlcs::ShmBuffer const& wlcs::Client::create_buffer(int width, int height)
{
    return impl->create_buffer(*this, width, height);
}

wlcs::Surface wlcs::Client::create_wl_shell_surface(int width, int height)
{
    return impl->create_wl_shell_surface(*this, width, height);
}

wlcs::Surface wlcs::Client::create_xdg_shell_v6_surface(int width, int height)
{
    return impl->create_xdg_shell_v6_surface(*this, width, height);
}

wlcs::Surface wlcs::Client::create_xdg_shell_stable_surface(int width, int height)
{
    return impl->create_xdg_shell_stable_surface(*this, width, height);
}

wlcs::Surface wlcs::Client::create_visible_surface(int width, int height)
{
    return impl->create_visible_surface(*this, width, height);
}

size_t wlcs::Client::output_count() const
{
    return impl->outputs.size();
}

wlcs::OutputState wlcs::Client::output_state(size_t index) const
{
    if (index > output_count())
        throw std::out_of_range("Invalid output index");

    return impl->outputs[index]->current;
}

void wlcs::Client::add_output_done_notifier(size_t index, std::function<void()> const& notifier)
{
    if (index > output_count())
        throw std::runtime_error("Invalid output index");

    impl->outputs[index]->done_notifiers.push_back(notifier);
}

wl_shell* wlcs::Client::shell() const
{
    return impl->the_shell();
}

zxdg_shell_v6* wlcs::Client::xdg_shell_v6() const
{
    return impl->the_xdg_shell_v6();
}

xdg_wm_base* wlcs::Client::xdg_shell_stable() const
{
    return impl->the_xdg_shell_stable();
}

wl_surface* wlcs::Client::keyboard_focused_window() const
{
    return impl->keyboard_focused_window();
}

wl_surface* wlcs::Client::window_under_cursor() const
{
    return impl->window_under_cursor();
}

wl_surface* wlcs::Client::touched_window() const
{
    return impl->touched_window();
}

std::pair<wl_fixed_t, wl_fixed_t> wlcs::Client::pointer_position() const
{
    return impl->pointer_position();
}

std::pair<wl_fixed_t, wl_fixed_t> wlcs::Client::touch_position() const
{
    return impl->touch_position();
}

std::optional<uint32_t> wlcs::Client::latest_serial() const
{
    return impl->latest_serial();
}

void wlcs::Client::add_pointer_enter_notification(PointerEnterNotifier const& on_enter)
{
    impl->add_pointer_enter_notification(on_enter);
}

void wlcs::Client::add_pointer_leave_notification(PointerLeaveNotifier const& on_leave)
{
    impl->add_pointer_leave_notification(on_leave);
}

void wlcs::Client::add_pointer_motion_notification(PointerMotionNotifier const& on_motion)
{
    impl->add_pointer_motion_notification(on_motion);
}

void wlcs::Client::add_pointer_button_notification(PointerButtonNotifier const& on_button)
{
    impl->add_pointer_button_notification(on_button);
}

void wlcs::Client::dispatch_until(std::function<bool()> const& predicate, std::chrono::seconds timeout)
{
    impl->dispatch_until(predicate, timeout);
}

void wlcs::Client::roundtrip()
{
    impl->server_roundtrip();
}

void wlcs::Client::flush()
{
    impl->client_flush();
}

void* wlcs::Client::bind_if_supported(wl_interface const& interface, VersionSpecifier const& version) const
{
    return impl->bind_if_supported(interface, version);
}

wl_pointer* wlcs::Client::the_pointer() const
{
    return impl->the_pointer();
}

class wlcs::Surface::Impl
{
public:
    Impl(Client& client)
        : surface_{wl_compositor_create_surface(client.compositor())},
          owner_{client}
    {
        wl_surface_add_listener(surface_, &surface_listener, this);
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

        for (auto const& callback: destruction_callbacks)
            callback();
        destruction_callbacks.clear();

        wl_surface_destroy(surface_);
    }

    ::wl_surface* surface() const
    {
        return surface_;
    }

    void attach_buffer(int width, int height)
    {
        auto& buffer = owner_.create_buffer(width, height);
        wl_surface_attach(surface_, buffer, 0, 0);
    }

    void add_frame_callback(std::function<void(uint32_t)> const& on_frame)
    {
        std::unique_ptr<std::function<void(uint32_t)>> holder{
            new std::function<void(uint32_t)>(on_frame)};

        auto callback = wl_surface_frame(surface_);

        pending_callbacks.push_back(std::make_pair(this, callback));

        wl_callback_add_listener(callback, &frame_listener, holder.release());
    }

    void attach_visible_buffer(int width, int height)
    {
        attach_buffer(width, height);
        auto surface_rendered = std::make_shared<bool>(false);
        add_frame_callback([surface_rendered](auto) { *surface_rendered = true; });
        wl_surface_commit(surface_);
        owner_.dispatch_until([surface_rendered]() { return *surface_rendered; });
    }

    void attach_toplevel(xdg_toplevel* toplevel) {
	    this->toplevel = toplevel;
    }

    xdg_toplevel* get_toplevel() { return toplevel; }

    void run_on_destruction(std::function<void()> callback)
    {
        destruction_callbacks.push_back(callback);
    }

    Client& owner() const
    {
        return owner_;
    }

    auto current_outputs() -> std::set<wl_output*> const&
    {
        return outputs;
    }

private:

    static std::vector<std::pair<Impl const*, wl_callback*>> pending_callbacks;
    std::set<wl_output*> outputs;

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

    static void on_enter(void* data, ::wl_surface* /*wl_surface*/, wl_output* output)
    {
        auto const self = static_cast<Impl*>(data);

        auto const inserted = self->outputs.insert(output);

        if (!inserted.second)
        {
            BOOST_THROW_EXCEPTION(std::runtime_error(
                "Got wl_surface.enter(wl_output@" +
                std::to_string(wl_proxy_get_id(reinterpret_cast<wl_proxy*>(output))) +
                ") for an output the surface is already on"));
        }
    }

    static void on_leave(void* data, ::wl_surface* /*wl_surface*/, wl_output* output)
    {
        auto const self = static_cast<Impl*>(data);

        auto const erased = self->outputs.erase(output);

        if (!erased)
        {
            BOOST_THROW_EXCEPTION(std::runtime_error(
                "Got wl_surface.leave(wl_output@" +
                std::to_string(wl_proxy_get_id(reinterpret_cast<wl_proxy*>(output))) +
                ") for an output the surface is not on"));
        }
    }

    static constexpr wl_surface_listener surface_listener{
        &on_enter,
        &on_leave,
    };

    struct wl_surface* const surface_;
    Client& owner_;
    std::vector<std::function<void()>> destruction_callbacks;
    struct xdg_toplevel* toplevel;
};

std::vector<std::pair<wlcs::Surface::Impl const*, wl_callback*>> wlcs::Surface::Impl::pending_callbacks;

constexpr wl_callback_listener wlcs::Surface::Impl::frame_listener;
constexpr wl_surface_listener wlcs::Surface::Impl::surface_listener;

wlcs::Surface::Surface(Client& client)
    : impl{std::make_unique<Impl>(client)}
{
}

wlcs::Surface::~Surface() = default;

wlcs::Surface::Surface(Surface&&) = default;

wlcs::Surface::operator ::wl_surface*() const
{
    return impl->surface();
}

void wlcs::Surface::attach_buffer(int width, int height)
{
    impl->attach_buffer(width, height);
}

void wlcs::Surface::add_frame_callback(std::function<void(int)> const& on_frame)
{
    impl->add_frame_callback(on_frame);
}

void wlcs::Surface::attach_visible_buffer(int width, int height)
{
    impl->attach_visible_buffer(width, height);
}

void wlcs::Surface::run_on_destruction(std::function<void()> callback)
{
    impl->run_on_destruction(callback);
}

void wlcs::Surface::attach_toplevel(struct xdg_toplevel* toplevel) {
	impl->attach_toplevel(toplevel);
}

struct xdg_toplevel* wlcs::Surface::get_toplevel() {
	return impl->get_toplevel();
}

wlcs::Client& wlcs::Surface::owner() const
{
    return impl->owner();
}

auto wlcs::Surface:: current_outputs() -> std::set<wl_output*> const&
{
    return impl->current_outputs();
}

class wlcs::Subsurface::Impl
{
public:
    Impl(Client& client, Surface& surface, Surface& parent)
        : subsurface_{wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent)},
          parent_{parent}
    {
    }

    ~Impl()
    {
        wl_subsurface_destroy(subsurface_);
    }

    struct wl_subsurface* const subsurface_;
    Surface& parent_;
};

wlcs::Subsurface wlcs::Subsurface::create_visible(Surface& parent, int x, int y, int width, int height)
{
    wlcs::Subsurface subsurface{parent};
    wl_subsurface_set_position(subsurface, x, y);

    subsurface.attach_buffer(width, height);

    bool surface_rendered{false};
    subsurface.add_frame_callback([&surface_rendered](auto) { surface_rendered = true; });
    wlcs::Surface* surface_ptr = &subsurface;
    while (surface_ptr)
    {
        wl_surface_commit(*surface_ptr);
        auto subsurface_ptr = dynamic_cast<wlcs::Subsurface*>(surface_ptr);
        if (subsurface_ptr)
            surface_ptr = &subsurface_ptr->parent();
        else
            surface_ptr = nullptr;
    }
    parent.owner().dispatch_until([&surface_rendered]() { return surface_rendered; });

    return subsurface;
}

wlcs::Subsurface::Subsurface(wlcs::Surface& parent)
    : Surface{parent.owner()},
      impl{std::make_unique<wlcs::Subsurface::Impl>(parent.owner(), *this, parent)}
{
}

wlcs::Subsurface::Subsurface(Subsurface &&) = default;

wlcs::Subsurface::~Subsurface() = default;

wlcs::Subsurface::operator wl_subsurface*() const
{
    return impl->subsurface_;
}

wlcs::Surface& wlcs::Subsurface::parent() const
{
    return impl->parent_;
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

wlcs::OutputState::OutputState(wl_output* output) : output{output}
{
}
