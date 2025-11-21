# WLCS Development Guide

## Architecture Overview

**What is WLCS**: A protocol-conformance test suite for Wayland compositors. Unlike previous test suites that use protocol extensions, WLCS uses compositor-provided integration modules loaded as shared libraries (.so files). Tests run in-process with the compositor, enabling easier debugging and consistent timing.

**Core Components**:
- `include/wlcs/display_server.h`: Integration API that compositors must implement (`WlcsServerIntegration`, `WlcsDisplayServer` structs)
- `src/`: Test infrastructure (test runner, client/server helpers, protocol wrappers)
- `tests/`: Protocol conformance tests for wl_compositor, xdg_shell, layer_shell, input methods, etc.
- `example/mir_integration.cpp`: Reference integration showing how Mir implements the WLCS API

**Key Pattern**: Compositors provide a `wlcs_server_integration` symbol in their integration .so. WLCS loads this and calls hooks to start/stop the compositor, create client sockets, inject input, etc.

## Test Infrastructure

**Base Classes** (see `include/in_process_server.h`):
- `InProcessServer`: GTest fixture that creates a `Server` instance. Use `SetUp()/TearDown()` explicitly
- `StartedInProcessServer`: Auto-starts the server in constructor. Most tests inherit from this

**Creating Test Clients**:
```cpp
wlcs::Client client{the_server()};  // Creates Wayland client connection
wlcs::Surface surface{client};       // Creates wl_surface
client.roundtrip();                  // Block until server processes requests
```

**Simulating Input** (see `include/wlcs/pointer.h`, `include/wlcs/touch.h`):
```cpp
auto pointer = the_server().create_pointer();
pointer.move_to(100, 200);  // Absolute coordinates
pointer.left_click();

auto touch = the_server().create_touch();
touch.down_at(50, 50);
touch.move_to(60, 60);
touch.up();
```

**Protocol Wrappers**: Headers like `layer_shell_v1.h`, `xdg_shell_stable.h` provide RAII wrappers around Wayland protocols with helper methods (e.g., `dispatch_until_configure()`).

**Parameterized Tests**: Many tests use `TEST_P` to run across multiple shell types or input devices. See `tests/subsurfaces.cpp` for the pattern using `AbstractInputDevice` abstraction.

## Code Style

Follow the [Canonical Mir C++ Guide](https://canonical-mir.readthedocs-hosted.com/stable/contributing/reference/cppguide/).

**Requirements**:
- C++20 required (`CMAKE_CXX_STANDARD 20`)
- Strict warnings: `-Werror` enabled by default (toggle with `WLCS_FATAL_COMPILE_WARNINGS`)

**Key Conventions**:
- Headers use include guards (`#ifndef WLCS_*_H_`)
- Wayland objects wrapped in RAII types (see `WlHandle<T>` in `wl_handle.h`)
- Helper timeouts: `wlcs::helpers::a_short_time()` (for negative tests), `wlcs::helpers::a_long_time()` (for operation timeouts)
- Test structure: Arrange (create surfaces/clients), Act (inject input/commit), Assert (check window focus/position)

**Testing Protocol Errors**: Use `expect_protocol_error.h` to verify compositor rejects invalid client requests:
```cpp
EXPECT_PROTOCOL_ERROR(client, &interface_name, error_code, {
    // code that should trigger error
});
```

## Common Tasks

**Adding Protocol Tests**:
1. Add `.xml` to `src/protocol/` if not present
2. Create wrapper class in `include/` (follow pattern from `layer_shell_v1.h`)
3. Implement wrapper in `src/` (generate protocol bindings in CMake)
4. Write tests in `tests/` inheriting from `StartedInProcessServer`

**Protocol Version Handling**: Use `VersionSpecifier` (see `version_specifier.h`) to bind interfaces:
```cpp
auto shell = client.bind_if_supported<xdg_wm_base>(wlcs::AtLeast(2));
```

## Security & CodeQL

**Do NOT check in CodeQL artifacts** - they bloat the repository and are regenerated in CI.

Add CodeQL database paths to `.gitignore`:
```
.cache
/build/
compile_commands.json
*.sarif
codeql-db/
```
