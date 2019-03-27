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

#ifndef WLCS_THREAD_PROXY_H_
#define WLCS_THREAD_PROXY_H_

#include <wayland-server-core.h>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <tuple>
#include <boost/throw_exception.hpp>

/*
 * ThreadProxy: Take a Callable and run it on the Wayland event loop.
 *
 * There are two main parts here: some template metaprogramming, and
 * the ThreadProxy class.
 *
 * ThreadProxy maintains the infrastructure for taking Callables
 * and invoking them on a Wayland event loop. The meat here is
 * in register_op(), which takes a Callable and returns a
 * new Callable that, when invoked, will result in the original
 * Callable being called on the Wayland event loop.
 *
 * The mechanism used for invoking on the Wayland event loop is a
 * socket: arguments are serialised to the socket, and then deserialised
 * when processed by the event loop.
 *
 * The template metaprogramming is mostly for manipulating parameter
 * packs: serialising them to and from linear buffers (so that they
 * can be passed across a socket), and invoking a Callable with a
 * tuple of args, and deducing the arguments and return value of
 * a Callable
 *
 */

namespace
{
/*
 * Deduce the argument types and return values of a Callable.
 *
 * Notably this only works for Callables with exactly one implementation
 * of operator(), otherwise template deduction will fail as it is unable
 * to select which operator() to operate on.
 *
 * This isn't an issue for what we're using this with, which is primarly
 * lambdas.
 */
template<typename Callable>
struct callable_traits : public callable_traits<decltype(&Callable::operator())> {};

template<typename Callable, typename Returns, typename... Args>
struct callable_traits<Returns(Callable::*)(Args...) const>
{
    using return_type = Returns;
    using args = typename std::tuple<Args...>;
};

/*
 * call_helper and call are downmarket versions of C++17's std::invoke()
 *
 * We don't yet require C++17, so downmarket it is!
 */
template<typename Returns, typename Callable, typename... Args, std::size_t... I>
Returns call_helper(Callable const& func, std::tuple<Args...> const& args, std::index_sequence<I...>)
{
    return func(std::get<I>(args)...);
}

template<typename Returns, typename Callable, typename... Args>
Returns call(Callable const& func, std::tuple<Args...> const& args)
{
    return call_helper<Returns>(func, args, std::index_sequence_for<Args...>{});
}

/*
 * tuple_from_buffer base case: To unpack a buffer containing no values,
 * return a std::tuple<>.
 */
template<
    typename... Args,
    typename std::enable_if<sizeof...(Args) == 0, int>::type = 0>
std::tuple<Args...> tuple_from_buffer(char*, std::tuple<Args...> const*)
{
    return std::tuple<Args...>{};
}

/*
 * Unpack a linear buffer into a std::tuple of its component types.
 */
template<typename Head, typename... Tail>
std::tuple<Head, Tail...> tuple_from_buffer(char* buffer, std::tuple<Head, Tail...> const*)
{
    // We need to memcpy out of the buffer as there's no guarantee data in the buffer has correct
    // alignment for this type.
    Head aligned_arg;
    ::memcpy(&aligned_arg, buffer, sizeof(Head));

    std::tuple<Tail...> const* type_deducer = nullptr;
    return std::tuple_cat(std::make_tuple(aligned_arg), tuple_from_buffer(buffer + sizeof(Head), type_deducer));
}

std::array<int, 2> setup_socketpair()
{
    std::array<int, 2> socket_fds;

    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, socket_fds.data()) < 0)
    {
        BOOST_THROW_EXCEPTION((
            std::system_error{
                errno,
                std::system_category(),
                "Failed to create Wayland thread communication socket"}));
    }

    return socket_fds;
}

namespace
{
constexpr ssize_t arguments_size()
{
    return 0;
}

template<typename T, typename... Remainder>
constexpr ssize_t arguments_size(T const& head, Remainder const&... tail)
{
    return sizeof(head) + arguments_size(tail...);
}

/*
 * pack_buffer base case: To pack no arguments into a buffer, do nothing.
 */
void pack_buffer(void*)
{
}

/*
 * Pack a any number of (TriviallyCopyable) arguments into a linear buffer.
 */
template<typename T, typename... Remainder>
void pack_buffer(char* buffer, T&& arg, Remainder&& ...args)
{
    memcpy(buffer, &arg, sizeof(arg));
    pack_buffer(buffer + sizeof(arg), std::forward<Remainder>(args)...);
}

/*
 * A bit more C++17 backporting...
 *
 * This bit of metaprogramming is in support of statically-asserting that
 * all the types passed through the proxy are TriviallyCopyable.
 */
template<class...>
struct conjunction : std::true_type {};

template<class Cond>
struct conjunction<Cond> : Cond {};

template<class Head, class... Tail>
struct conjunction<Head, Tail...> :
    std::conditional_t<bool(Head::value), conjunction<Tail...>, Head> {};

template<typename... Args>
constexpr bool all_args_are_trivially_copyable(std::tuple<Args...> const*)
{
    return conjunction<std::is_trivially_copyable<Args>...>::value;
}
}

/*
 * ThreadProxy: a mechanism to take a functor from any thread, and turn it into
 * a functor that runs on the Wayland event loop.
 *
 * Theory of operation:
 * The inter-thread channel used is a SOCK_SEQPACKET socket.
 *
 * register_op() takes an (almost) arbitrary Callable, wraps it in a
 * std::function<> that recieves a pointer to a buffer of arguments,
 * unpacks the buffer into a std::tuple, then invokes the original
 * Callable, and adds this function to the handler array.
 *
 * register_op() then *returns* a function with the same signature that
 * copies the array index of the relevant handler into a falt buffer,
 * marshalls the arguments passed in into the buffer and then pushes that buffer
 * into the socket.
 *
 * On the Wayland event loop side, we add a fd event_source to the event_loop
 * which reads from the Wayland end of the socket, extracts the index into the
 * handler array, then calls the handler with the rest of the message buffer.
 */
class ThreadProxy : public std::enable_shared_from_this<ThreadProxy>
{
private:

    template<typename... Args>
    void send_message(uint32_t opcode, Args&& ...args) const
    {
        char buffer[max_message_size];
        ssize_t const message_size = sizeof(uint32_t) + arguments_size(args...);

        *reinterpret_cast<uint32_t*>(buffer) = opcode;
        pack_buffer(buffer + sizeof(uint32_t), std::forward<Args>(args)...);

        auto const written = send(fds[Fd::Client], buffer, message_size, MSG_DONTWAIT);

        if (written < message_size)
        {
            if (written < 0)
            {
                BOOST_THROW_EXCEPTION((
                    std::system_error{
                        errno,
                        std::system_category(),
                        "Failed to send message to Wayland thread"}));
            }
            BOOST_THROW_EXCEPTION((
                std::runtime_error{
                    "Failed to send whole message to Wayland thread"}));
        }
    }

    template<typename T>
    T wait_for_reply() const
    {
        T buffer;
        // Wait for it to be processed
        auto const read = recv(fds[Fd::Client], &buffer, sizeof(buffer), 0);
        if (read < static_cast<ssize_t>(sizeof(buffer)))
        {
            if (read < 0)
            {
                BOOST_THROW_EXCEPTION((
                    std::system_error{
                        errno,
                        std::system_category(),
                        "Failed to receive reply from Wayland thread"}));
            }
            BOOST_THROW_EXCEPTION((
                std::runtime_error{
                    "Received short reply from Wayland thread"}));
        }
        return buffer;
    }

    template<typename... Args>
    auto make_send_functor(uint32_t opcode, std::tuple<Args...> const*) const
    {
        return
            [me = this->shared_from_this(), opcode](Args&& ...args)
            {
                // We technically don't need to serialise the send/receive, but
                // it's a bit easier if only one request is in flight at once.
                std::lock_guard<std::mutex> lock{me->message_serialiser};
                me->send_message(opcode, std::forward<Args>(args)...);
                me->wait_for_reply<char>();
            };
    }

    template<typename Returns, typename... Args>
    auto make_send_functor(uint32_t opcode, std::tuple<Args...> const*) const
    {
        return
            [me = this->shared_from_this(), opcode](Args&& ...args)
            {
                // We technically don't need to serialise the send/receive, but
                // it's a bit easier if only one request is in flight at once.
                std::lock_guard<std::mutex> lock{me->message_serialiser};
                me->send_message(opcode, std::forward<Args>(args)...);
                return me->wait_for_reply<Returns>();
            };
    }

    template<
        typename Callable,
        typename... Args,
        typename
        std::enable_if_t<
            std::is_same<
                typename callable_traits<typename std::decay<Callable>::type>::return_type,
                void>::value,
            int> = 0>
    auto make_recv_functor(Callable&& handler, std::tuple<Args...> const*)
    {
        return
            [this, handler = std::move(handler)](void* data)
            {
                std::tuple<Args...> const* type_resolver = nullptr;
                auto const args = tuple_from_buffer(static_cast<char*>(data), type_resolver);

                call<void>(handler, args);

                // void functions send a dummy char, to notify the other end that
                // the call has completed.
                char const dummy{0};
                send(fds[Fd::Wayland], &dummy, sizeof(dummy), 0);
            };
    }

    template<
        typename Callable,
        typename... Args,
        typename
        std::enable_if_t<
            !std::is_same<
                typename callable_traits<typename std::decay<Callable>::type>::return_type,
                void>::value, int> = 0>
    auto make_recv_functor(Callable&& handler, std::tuple<Args...> const*)
    {
        return
            [this, handler = std::move(handler)](void* data)
            {
                using traits = callable_traits<typename std::decay<Callable>::type>;

                std::tuple<Args...> const* type_resolver = nullptr;
                auto const args = tuple_from_buffer(static_cast<char*>(data), type_resolver);

                auto const val = call<typename traits::return_type>(handler, args);

                send(this->fds[Fd::Wayland], &val, sizeof(val), 0);
            };
    }
public:
    template<
        typename Callable,
        typename
        std::enable_if_t<
            std::is_same<
                typename callable_traits<typename std::decay<Callable>::type>::return_type,
                void>::value, int> = 0>
    auto register_op(Callable handler)
    {
        using traits = callable_traits<typename std::decay<Callable>::type>;
        // This is technically too restrictive; std::tuple<Args...> might have a size greater
        // than the sum of its parts.
        static_assert(
            sizeof(typename traits::args) < max_arguments_size,
            "Attempt to call function with too many arguments; bump max_message_size");


        // Get template deduction to work by passing an (unused) argument of type
        // std::tuple<Args...>*; we can't access Args... ourselves here.
        constexpr typename traits::args const* type_resolver = nullptr;

        static_assert(
            all_args_are_trivially_copyable(type_resolver),
            "All arguments of register_op must be TriviallyCopyable as they must support memcpy");

        auto const next_opcode = handlers.size();
        handlers.emplace_back(make_recv_functor(std::move(handler), type_resolver));

        return make_send_functor(next_opcode, type_resolver);
    }

    template<
        typename Callable,
        typename
        std::enable_if_t<
            !std::is_same<
                typename callable_traits<typename std::decay<Callable>::type>::return_type,
                void>::value, int> = 0>
    auto register_op(Callable handler)
    {
        using traits = callable_traits<typename std::decay<Callable>::type>;
        static_assert(
            sizeof(typename traits::args) < max_arguments_size,
            "Attempt to call function with too many arguments; bump max_message_size");
        static_assert(
            sizeof(typename traits::return_type) < max_message_size,
            "Attempt to call function with too large return value; bump max_message_size");

        // Get template deduction to work by passing an (unused) argument of type
        // std::tuple<Args...>; we can't access Args... ourselves here.
        constexpr typename traits::args const* type_resolver = nullptr;
        static_assert(
            all_args_are_trivially_copyable(type_resolver),
            "All arguments of register_op must be TriviallyCopyable as they must support memcpy");

        auto const next_opcode = handlers.size();
        handlers.emplace_back(make_recv_functor(std::move(handler), type_resolver));

        return make_send_functor<typename traits::return_type>(next_opcode, type_resolver);
    }

    explicit ThreadProxy(struct wl_event_loop* event_loop)
        : fds{setup_socketpair()},
          source{
              wl_event_loop_add_fd(
                  event_loop,
                  fds[Fd::Wayland],
                  WL_EVENT_READABLE,
                  &ThreadProxy::socket_readable,
                  this)},
        // We have to manually implement the destructor() thunk rather than using register_op because
        // the send_functor returned by register_op takes shared reference to this, care of
        // shared_from_this(). But during construction `this` is not a shared_ptr, so shared_from_this()
        // isn't usable.
          destructor{
              [this]()
              {
                  send_message(0);
              }}
    {
        // We must run wl_event_source_remove() on the Wayland mainloop, too.
        handlers.emplace_back(
            [this](void*)
            {
                wl_event_source_remove(source);
            });
    }

    ~ThreadProxy()
    {
        destructor();
        close(fds[0]);
        close(fds[1]);
    }

private:
    static int socket_readable(int fd, uint32_t /*mask*/, void* data) noexcept
    {
        auto const& me = *static_cast<ThreadProxy*>(data);
        char buffer[max_message_size];

        recv(fd, buffer, sizeof(buffer), 0);

        auto const opcode = *reinterpret_cast<uint32_t*>(buffer);
        me.handlers[opcode](buffer + sizeof(uint32_t));

        return 0;
    }

    static constexpr ssize_t max_arguments_size{1024};
    static constexpr ssize_t max_message_size = max_arguments_size + sizeof(uint32_t);
    enum Fd
    {
        Wayland = 0,
        Client
    };
    std::array<int, 2> const fds;
    struct wl_event_source* const source;
    std::mutex mutable message_serialiser;
    std::vector<std::function<void(void*)>> handlers;
    std::function<void()> const destructor;
};
}


#endif //WLCS_THREAD_PROXY_H_
