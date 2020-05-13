#include "generated/gtk-primary-selection-client.h"
#include "generated/primary-selection-unstable-v1-client.h"
#include "generated/xdg-output-unstable-v1-client.h"
#include "layer_shell_v1.h"

#include "wl_proxy.h"
#include "in_process_server.h"

// We have to be inside the wlcs namespace due to GCC bug 56480 (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480)
// The namespace can be moved to each declaration once we drop 16.04
namespace wlcs
{

#define GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(name) \
    template<> \
    WlProxy<name>::WlProxy(Client const& client, uint32_t min_version) \
        : proxy{static_cast<name*>(client.bind_if_supported(name##_interface, min_version))}, \
          destroy{name ##_destroy} \
    { \
    }

GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_data_device_manager)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(xdg_wm_base)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zwp_primary_selection_device_manager_v1)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zxdg_output_manager_v1)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(gtk_primary_selection_device_manager)
GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zwlr_layer_shell_v1)

#undef GLOBAL_DEFAULT_DESTROY_PROXY_CONSTRUCTOR

#define DEFAULT_DESTROY_PROXY_CONSTRUCTOR(name) \
    template<> \
    WlProxy<name>::WlProxy(name* proxy) \
        : proxy{proxy}, \
          destroy{name##_destroy} \
    { \
    }

DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_surface)
DEFAULT_DESTROY_PROXY_CONSTRUCTOR(wl_subsurface)
DEFAULT_DESTROY_PROXY_CONSTRUCTOR(zwlr_layer_surface_v1)

}

#undef DEFAULT_DESTROY_PROXY_CONSTRUCTOR
