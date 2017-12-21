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

#ifndef WLCS_IN_PROCESS_SERVER_H_
#define WLCS_IN_PROCESS_SERVER_H_

#include <gtest/gtest.h>

#include <wayland-client.h>
#include <functional>

struct WlcsPointer;

namespace wlcs
{

class Pointer
{
public:
    ~Pointer();
    Pointer(Pointer&&);

    void move_to(int x, int y);

private:
    friend class Server;
    Pointer(WlcsPointer* raw_device);

    class Impl;
    std::unique_ptr<Impl> impl;
};

class Surface;

class Server
{
public:
    Server(int argc, char const** argv);
    ~Server();

    int create_client_socket();

    Pointer create_pointer();

    void move_surface_to(Surface& surface, int x, int y);

    void start();
    void stop();
private:
    class Impl;
    std::unique_ptr<Impl> const impl;
};

class Client;

class Surface
{
public:
    Surface(Client& client);
    ~Surface();

    Surface(Surface&& other);

    operator wl_surface*() const;

    void add_frame_callback(std::function<void(int)> const& on_frame);

    bool has_focus() const;
    std::pair<int, int> pointer_position() const;

    Client& owner() const;
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

class Client
{
public:
    Client(Server& server);
    ~Client();

    // Accessors

    operator wl_display*() const;

    wl_compositor* compositor() const;
    wl_shm* shm() const;

    Surface create_visible_surface(int width, int height);

    wl_surface* focused_window() const;
    std::pair<wl_fixed_t, wl_fixed_t> pointer_position() const;

    void dispatch_until(std::function<bool()> const& predicate);
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

}

#endif //WLCS_IN_PROCESS_SERVER_H_
