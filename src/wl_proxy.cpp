#include "generated/gtk-primary-selection-client.h"
#include "generated/primary-selection-unstable-v1-client.h"
#include "generated/xdg-output-unstable-v1-client.h"

#include "wl_proxy.h"
#include "in_process_server.h"

#define GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(name) \
    template<> \
    wlcs::WlProxy<name>::WlProxy(Client const& client) \
        : WlProxy(client.bind_if_supported(name##_interface, name ##_destroy)) \
    { \
    }

GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_subcompositor)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_data_device_manager)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(xdg_wm_base)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zwp_primary_selection_device_manager_v1)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zxdg_output_manager_v1)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(gtk_primary_selection_device_manager)

#undef GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR

#define DEFAULT_DESTROY_PROXY_CONSTRUCTOR(name) \
    template<> \
    wlcs::WlProxy<name>::WlProxy(name* proxy) \
        : proxy{proxy}, \
          destroy{name##_destroy} \
    { \
    }

DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_surface)

#undef DEFAULT_DESTROY_PROXY_CONSTRUCTOR
