#include "generated/gtk-primary-selection-client.h"
#include "generated/primary-selection-unstable-v1-client.h"
#include "generated/xdg-output-unstable-v1-client.h"

#include "wl_proxy.h"
#include "in_process_server.h"

template<>
wlcs::WlProxy<
    wl_subcompositor>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    wl_subcompositor_interface,
    wl_subcompositor_destroy))
{
}

template<>
wlcs::WlProxy<
    wl_data_device_manager>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    wl_data_device_manager_interface,
    wl_data_device_manager_destroy))
{
}

template<>
wlcs::WlProxy<
    xdg_wm_base>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    xdg_wm_base_interface,
    xdg_wm_base_destroy))
{
}

template<>
wlcs::WlProxy<
    zwp_primary_selection_device_manager_v1>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    zwp_primary_selection_device_manager_v1_interface,
    zwp_primary_selection_device_manager_v1_destroy))
{
}

template<>
wlcs::WlProxy<
    zxdg_output_manager_v1>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    zxdg_output_manager_v1_interface,
    zxdg_output_manager_v1_destroy))
{
}

template<>
wlcs::WlProxy<
    gtk_primary_selection_device_manager>::WlProxy(Client const& client) : WlProxy(client.bind_if_supported(
    gtk_primary_selection_device_manager_interface,
    gtk_primary_selection_device_manager_destroy))
{
}

template<>
wlcs::WlProxy<wl_surface>::WlProxy(wl_surface* proxy)
    : proxy{proxy},
      destroy{wl_surface_destroy}
{
}
