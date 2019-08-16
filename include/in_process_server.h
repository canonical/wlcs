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

#ifndef WLCS_IN_PROCESS_SERVER_H_
#define WLCS_IN_PROCESS_SERVER_H_

#include "generated/gtk-primary-selection-client.h"
#include "generated/primary-selection-unstable-v1-client.h"
#include "generated/wayland-client.h"
#include "generated/xdg-shell-unstable-v6-client.h"
#include "generated/xdg-shell-client.h"

#include <gtest/gtest.h>

#include <memory>
#include <functional>
#include <experimental/optional>
#include <unordered_map>
#include <chrono>

#include "shared_library.h"

#include <wayland-client.h>

struct WlcsPointer;
struct WlcsTouch;
struct WlcsServerIntegration;

struct zwlr_layer_shell_v1;

namespace wlcs
{

class Pointer
{
public:
    ~Pointer();
    Pointer(Pointer&&);

    void move_to(int x, int y);
    void move_by(int dx, int dy);

    void button_down(int button);
    void button_up(int button);
    void click(int button);

    void left_button_down();
    void left_button_up();
    void left_click();

private:
    friend class Server;
    template<typename Proxy>
    Pointer(
        WlcsPointer* raw_device,
        std::shared_ptr<Proxy> const& proxy,
        std::shared_ptr<void const> const& keep_dso_loaded);

    class Impl;
    std::unique_ptr<Impl> impl;
};

class Touch
{
public:
    ~Touch();
    Touch(Touch&&);

    void down_at(int x, int y);
    void move_to(int x, int y);
    void up();

private:
    friend class Server;
    template<typename Proxy>
    Touch(
        WlcsTouch* raw_device,
        std::shared_ptr<Proxy> const& proxy,
        std::shared_ptr<void const> const& keep_dso_loaded);

    class Impl;
    std::unique_ptr<Impl> impl;
};

class Surface;

class Server
{
public:
    Server(
        std::shared_ptr<WlcsServerIntegration const> const& module,
        int argc,
        char const** argv);
    ~Server();

    int create_client_socket();

    Pointer create_pointer();
    Touch create_touch();

    void move_surface_to(Surface& surface, int x, int y);

    void start();
    void stop();

    std::shared_ptr<const std::unordered_map<std::string, uint32_t>> supported_extensions();
private:
    class Impl;
    std::unique_ptr<Impl> const impl;
};

class Client;

class Surface
{
public:
    explicit Surface(Client& client);
    virtual ~Surface();

    Surface(Surface&& other);

    operator wl_surface*() const;

    void attach_buffer(int width, int height);
    void add_frame_callback(std::function<void(int)> const& on_frame);
    void attach_visible_buffer(int width, int height);

    bool has_focus() const;
    std::pair<int, int> pointer_position() const;

    Client& owner() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

class Subsurface: public Surface
{
public:
    static Subsurface create_visible(Surface& parent, int x, int y, int width, int height);

    Subsurface(Surface& parent);
    Subsurface(Subsurface &&);
    ~Subsurface();

    operator wl_subsurface*() const;

    Surface& parent() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

class ShmBuffer
{
public:
    ShmBuffer(Client& client, int width, int height);
    ~ShmBuffer();

    ShmBuffer(ShmBuffer&& other);

    operator wl_buffer*() const;

    void add_release_listener(std::function<bool()> const &on_release);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

struct OutputState
{
    OutputState(wl_output* output)
        : output{output}
    {
    }

    wl_output* output;
    std::experimental::optional<std::pair<int, int>> geometry_position;
    std::experimental::optional<std::pair<int, int>> mode_size;
    std::experimental::optional<int> scale;
};

class Client
{
public:
    Client(Server& server);
    ~Client();

    // Accessors

    operator wl_display*() const;

    wl_compositor* compositor() const;
    wl_subcompositor* subcompositor() const;
    wl_shm* shm() const;
    wl_data_device_manager* data_device_manager() const;
    wl_seat* seat() const;
    struct zwp_primary_selection_device_manager_v1* primary_selection_device_manager() const;
    struct gtk_primary_selection_device_manager* gtk_primary_selection_device_manager() const;

    void run_on_destruction(std::function<void()> callback);

    ShmBuffer const& create_buffer(int width, int height);
    Surface create_wl_shell_surface(int width, int height);
    Surface create_xdg_shell_v6_surface(int width, int height);
    Surface create_xdg_shell_stable_surface(int width, int height);
    Surface create_visible_surface(int width, int height);

    size_t output_count() const;
    OutputState output_state(size_t index) const;

    wl_shell* shell() const;
    zxdg_shell_v6* xdg_shell_v6() const;
    xdg_wm_base* xdg_shell_stable() const;
    zwlr_layer_shell_v1* layer_shell_v1() const;
    wl_surface* focused_window() const;
    wl_surface* touched_window() const;
    std::pair<wl_fixed_t, wl_fixed_t> pointer_position() const;
    std::pair<wl_fixed_t, wl_fixed_t> touch_position() const;
    bool pointer_events_clean() const; ///< If we have gotten a frame event since the last pointer event

    using PointerEnterNotifier =
        std::function<bool(wl_surface*, wl_fixed_t x, wl_fixed_t y)>;
    using PointerLeaveNotifier =
        std::function<bool(wl_surface*)>;
    using PointerMotionNotifier =
        std::function<bool(wl_fixed_t x, wl_fixed_t y)>;
    using PointerButtonNotifier =
        std::function<bool(uint32_t serial, uint32_t button, bool is_down)>;
    void add_pointer_enter_notification(PointerEnterNotifier const& on_enter);
    void add_pointer_leave_notification(PointerLeaveNotifier const& on_leave);
    void add_pointer_motion_notification(PointerMotionNotifier const& on_motion);
    void add_pointer_button_notification(PointerButtonNotifier const& on_button);

    void* acquire_interface(std::string const& name, wl_interface const* interface, uint32_t version);

    void dispatch_until(
        std::function<bool()> const& predicate,
        std::chrono::seconds timeout = std::chrono::seconds{10});

    void roundtrip();
private:
    class Impl;
    std::unique_ptr<Impl> const impl;
};

class ProtocolError : public std::system_error
{
public:
    ProtocolError(wl_interface const* interface, uint32_t code)
        : std::system_error(EPROTO, std::system_category(), "Wayland protocol error"),
        interface_{interface},
        code_{code},
        message{
            std::string{"Wayland protocol error: "} +
            std::to_string(code) +
            " on interface " +
            interface_->name +
            " v" +
            std::to_string(interface->version)}
    {
    }

    char const* what() const noexcept override
    {
        return message.c_str();
    }

    uint32_t error_code() const
    {
        return code_;
    }

    wl_interface const* interface() const
    {
        return interface_;
    }

private:
    wl_interface const* const interface_;
    uint32_t const code_;
    std::string const message;
};

class ExtensionExpectedlyNotSupported : public std::runtime_error
{
public:
    ExtensionExpectedlyNotSupported(char const* extension, uint32_t version);
};

class Timeout : public std::runtime_error
{
public:
    explicit Timeout(char const* message);
};

class InProcessServer : public testing::Test
{
public:
    InProcessServer();

    void SetUp() override;
    void TearDown() override;

    Server& the_server();
private:
    Server server;
};

class StartedInProcessServer : public InProcessServer
{
public:
    StartedInProcessServer() { InProcessServer::SetUp(); }
    ~StartedInProcessServer() { InProcessServer::TearDown(); }

    void SetUp() override {}
    void TearDown() override {}
};

// Check the server expects to support an interface
class CheckInterfaceExpected : private Client
{
public:
    CheckInterfaceExpected(Server& server, wl_interface const& interface);
};

}

#endif //WLCS_IN_PROCESS_SERVER_H_
