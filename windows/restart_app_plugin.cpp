#include "restart_app_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>
#include <shellapi.h>
#include <iostream>
#include <vector>

namespace restart_app {

// static
void RestartAppPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "restart",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<RestartAppPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

// This is the function that will be called
    void RestartAppPlugin::restartApp() {
        std::cout << "Attempting to restart the application..." << std::endl;

        // 1. Get the path to the current executable
        std::vector<wchar_t> exePath(MAX_PATH);
        if (GetModuleFileName(NULL, exePath.data(), static_cast<DWORD>(exePath.size())) == 0) {
            DWORD error = GetLastError();
            std::cerr << "Failed to get current executable path. Error: " << error << std::endl;
            // Optionally, inform Flutter about the failure via the MethodChannel result
            return;
        }

        std::wstring commandLine = exePath.data();
        std::cout << "Executable path: " << commandLine.c_str() << std::endl;

        // 2. Prepare to launch a new instance using ShellExecuteEx
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS; // We want the process handle if needed, though not strictly used here for restart
        sei.hwnd = NULL;                     // No owner window
        sei.lpVerb = L"open";                // Verb to use (open is standard for executables)
        sei.lpFile = commandLine.c_str();    // Path to the executable
        sei.lpParameters = L"";              // Any command-line parameters for the new instance (if any)
        sei.lpDirectory = NULL;              // Default working directory
        sei.nShow = SW_SHOWNORMAL;           // How to show the new window

        std::cout << "Launching new application instance..." << std::endl;
        if (ShellExecuteEx(&sei)) {
            std::cout << "Successfully initiated new application instance." << std::endl;

            // 3. Terminate the current instance
            // This is a forceful exit. Ensure all cleanup (saving data, releasing resources)
            // that can be done before this point has been completed.
            std::cout << "Terminating current application instance." << std::endl;
            ExitProcess(0); // 0 indicates successful exit
        } else {
            DWORD error = GetLastError();
            std::cerr << "Failed to launch new application instance using ShellExecuteEx. Error: " << error << std::endl;
            // Optionally, inform Flutter about the failure via the MethodChannel result
        }
    }

RestartAppPlugin::RestartAppPlugin() {}

RestartAppPlugin::~RestartAppPlugin() {}

void RestartAppPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    result->Success(flutter::EncodableValue(version_stream.str()));
  }
  else if (method_call.method_name().compare("restartApp") == 0) { // Check for "restartApp" method
      restartApp(); // Call our new function
      result->Success(); // Indicate success (or you could return a value if needed)
  }
  else {
      result->NotImplemented();
  }
}

}  // namespace restart_app
