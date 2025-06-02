#include "include/restart_app/restart_app_plugin.h"

#include <flutter/plugin_registrar_windows.h>

#include "restart_app_plugin.h"

void RestartAppPluginRegisterWithRegistrar(
        FlutterDesktopPluginRegistrarRef registrar) {
    restart_app::RestartAppPlugin::RegisterWithRegistrar(
            flutter::PluginRegistrarManager::GetInstance()
                    ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}