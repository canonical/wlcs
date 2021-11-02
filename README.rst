==============================
Wayland Conformance Test Suite
==============================

``wlcs`` aspires to be a protocol-conformance-verifying test suite usable by
Wayland compositor implementors.

It is growing out of porting the existing Weston test suite to be run
in `Mir's <https://github.com/MirServer/mir>`_ test suite,
but it is designed to be usable by any compositor.

There have been a number of previous attempts at a Wayland compositor test
suite - the `Wayland Functional Integration Test Suite <https://github.com/intel/wayland-fits>`_,
and the `Weston test suite <https://wayland.freedesktop.org/testing.html#heading_toc_j_3>`_
are a couple of examples.

What sets ``wlcs`` apart from the graveyard of existing test suites is its
integration method.

Previous test suites have used a Wayland protocol extension
to interrogate the compositor state. This obviously requires that compositors
implement that protocol, means that tests only have access to information
provided by that protocol, and the IPC means that there isn't a canonical
happens-before ordering available.

Instead, ``wlcs`` relies on compositors providing an integration module,
providing ``wlcs`` with API hooks to start a compositor, connect a client,
move a window, and so on. This makes both writing and debugging tests easier -
the tests are (generally) in the same address space as the compositor, so there
is a consistent global clock available, it's easier to poke around in
compositor internals, and standard debugging tools can follow control flow from
the test client to the compositor and back again.

Usage
-----

In order for ``wlcs`` to test your compositor you need to provide an
integration module. This needs to implement the interfaces found in
``include/wlcs``. If your integration module is
``awesome_compositor_wlcs_integration.so``, then running ``wlcs
awesome_compositor_wlcs_integration.so`` will load and run all the tests.

Development
-----------

``wlcs`` requires a small number of hooks into your compositor to drive the
tests - it needs to know how to start and stop the mainloop, to get an fd to
connect a client to, and so on.

To access these hooks, ``wlcs`` looks for a symbol ``wlcs_server_integration``
of type ``struct WlcsServerIntegration const`` (defined in
``include/wlcs/display_server.h``) in your integration module.
