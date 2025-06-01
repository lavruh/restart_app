#include "include/restart_app/restart_app_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <glib.h>
#include <cstdlib>

#include <cstring>

#include "restart_app_plugin_private.h"

#define RESTART_APP_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), restart_app_plugin_get_type(), \
                              RestartAppPlugin))

struct _RestartAppPlugin {
    GObject parent_instance;
    FlMethodChannel* restart;
};

G_DEFINE_TYPE(RestartAppPlugin, restart_app_plugin, g_object_get_type())

static void restartApp_method_call_handler(FlMethodChannel* channel,
                                           FlMethodCall* method_call,
                                           gpointer user_data) {
    g_autoptr(FlMethodResponse) response = nullptr;
    if (strcmp(fl_method_call_get_name(method_call), "restartApp") == 0) {
        response = restartApp_impl();
    } else {
        response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
    }

    g_autoptr(GError) error = nullptr;
    if (!fl_method_call_respond(method_call, response, &error)) {
        g_warning("Failed to send response: %s", error->message);
    }
}

// Helper function to get the current executable's path
static std::string get_executable_path() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        return std::string(result, count);
    }
    g_warning("Could not determine executable path via /proc/self/exe.");
    return ""; // Or handle error appropriately
}

// Helper function to get current command line arguments
static std::vector<std::string> get_current_args() {
    std::vector<std::string> args;
    std::ifstream cmdline_file("/proc/self/cmdline");
    std::string arg;

    if (cmdline_file.is_open()) {
        while (std::getline(cmdline_file, arg, '\0')) {
            if (!arg.empty()) {
                args.push_back(arg);
            }
        }
        cmdline_file.close();
    } else {
        g_warning("Could not open /proc/self/cmdline to get arguments.");
        const gchar* prgname = g_get_prgname();
        if (prgname) {
            args.push_back(prgname);
        }
    }
    return args;
}


// Implementation of the restartApp functionality
static FlMethodResponse* restartApp_impl() {
    g_print("Attempting to restart application...\n");

    std::string exe_path = get_executable_path();
    if (exe_path.empty()) {
        g_warning("Failed to get executable path. Restart aborted.");
        return FL_METHOD_RESPONSE(fl_method_error_response_new(
                "RESTART_FAILED", "Could not determine executable path.", nullptr));
    }

    std::vector<std::string> current_args_str = get_current_args();
    if (current_args_str.empty()) {
        // If we couldn't get args but have the exe_path, we can try with just the exe_path
        g_warning("Could not retrieve current arguments. Attempting restart with executable path only.");
        current_args_str.push_back(exe_path); // Use exe_path as the first argument
    }


    // Convert std::vector<std::string> to char* argv[] for execv
    std::vector<char*> argv;
    for (const auto& s : current_args_str) {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr); // argv must be null-terminated

    g_print("Executable path: %s\n", exe_path.c_str());
    g_print("Arguments for new process:\n");
    for (size_t i = 0; argv[i] != nullptr; ++i) {
        g_print("  argv[%ld]: %s\n", i, argv[i]);
    }

    // Fork the process
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork");
        g_warning("Fork failed: %s. Restart aborted.", g_strerror(errno));
        return FL_METHOD_RESPONSE(fl_method_error_response_new(
                "RESTART_FAILED", "Fork failed.", fl_value_new_string(g_strerror(errno))));
    } else if (pid == 0) {
        g_print("Child process (PID: %d) attempting to execv...\n", getpid());

        if (execv(exe_path.c_str(), argv.data()) == -1) {
            perror("execv");
            g_warning("execv failed in child process: %s", g_strerror(errno));
            _exit(EXIT_FAILURE); // Use _exit in child after fork to avoid running atexit handlers etc.
        }
    } else {
        g_print("Parent process (PID: %d) waiting for child (PID: %d) to start, then exiting...\n", getpid(), pid);
        g_print("Parent process exiting to allow new instance to take over.\n");
        exit(EXIT_SUCCESS); // Or use a more Flutter-specific exit if available/needed.
    }
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
            "RESTART_UNEXPECTED_STATE", "Should have exited or exec'd.", nullptr));
}


FlMethodResponse* get_platform_version() {
    struct utsname uname_data = {};
    uname(&uname_data);
    g_autofree gchar *version = g_strdup_printf("Linux %s", uname_data.version);
    g_autoptr(FlValue) result = fl_value_new_string(version);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

static void restart_app_plugin_dispose(GObject* object) {
    RestartAppPlugin* self = RESTART_APP_PLUGIN(object);
    g_clear_object(&self->restart); // Release the channel
    G_OBJECT_CLASS(restart_app_plugin_parent_class)->dispose(object);
}

static void restart_app_plugin_class_init(RestartAppPluginClass* klass) {
    G_OBJECT_CLASS(klass)->dispose = restart_app_plugin_dispose;
}

static void restart_app_plugin_init(RestartAppPlugin* self) {}


void restart_app_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
    RestartAppPlugin* plugin = RESTART_APP_PLUGIN(
            g_object_new(restart_app_plugin_get_type(), nullptr));

    g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();

    // Create and set up the "restart" method channel
    plugin->restart =
            fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                                  "restart", // Channel name
                                  FL_METHOD_CODEC(codec));
    fl_method_channel_set_method_call_handler(plugin->restart,
                                              restartApp_method_call_handler, // Specific handler
                                              g_object_ref(plugin), // User data
                                              g_object_unref);      // Destroy notify

    g_object_unref(plugin); // Unref the plugin as the channel now holds a reference
}