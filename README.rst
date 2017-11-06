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

What sets ``libwlcs`` apart from the graveyard of existing test suites is its
integration method.

Previous test suites have used a Wayland protocol extension
to interrogate the compositor state. This obviously requires that compositors
implement that protocol, means that tests only have access to information
provided by that protocol, and the IPC means that there isn't a canonical
happens-before ordering available.

Instead, ``libwlcs``, as its name suggets, is a library for building your
test suite. This makes both writing and debugging tests easier - the tests
are (generally) in the same address space as the compositor, so there is a
consistent global clock available, it's easier to poke around in compositor
internals, and standard debugging tools can follow control flow from the
test client to the compositor and back again.

Usage
-----

``libwlcs`` provides the tests and a :code:`main()` function. All that
a compositor needs to do is to provide implementations of the weak symbols found
in ``display_server.h`` that are used to drive the compositor.

The ``Mir`` integration is provided as an example in ``examples/mir_integration.cpp``.
