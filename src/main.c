#include "args.h"
#include "sandbox.h"
#include "util.h"

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


bool g_verbose = false;
bool g_source_is_ephemeral = false;
const char* g_mount_base_path;


static argument_t arguments = { NULL };

static void cleanup() {
    bool cleanup_success = cleanup_sandbox();

    if (cleanup_success) {
        remove_directory_recursive(g_mount_base_path);
        if (g_verbose) {
            printf("Successfully removed mount staging directory %s\n", g_mount_base_path);
        }
    }
    else {
        eprintf("Sandbox cleanup failed, skipping base directory removal %s\n", g_mount_base_path);
    }

    free((void*)arguments.source_path); // manual path or from realpath(), free in any case
    free((void*)g_mount_base_path);
}

static void signal_handler(int sig) {
    eprintf("\nReceived signal %d, cleaning up...\n", sig);
    cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}


int main(int argc, char *argv[]) {
    if (detect_sandbox()) return EXIT_SUCCESS;

    if (geteuid() != 0) {
        fail("FATAL: "APP_NAME" does not have elevated privileges required for mount operations.\n"
             "Executable should be owned by root with setuid bit enabed.\n")
    }

    parse_args(argc, argv, &arguments);
    g_verbose = arguments.verbose;

    if (arguments.mount_id == NULL) {
        arguments.mount_id = current_timestamp();
    }
    else {
        char* bad_char=strpbrk(arguments.mount_id, "/\n");
        if (bad_char != NULL) {
            uint position = bad_char-arguments.mount_id;
            fail("Illegal character in provided mount ID as position %d\n", position)
        }
    }

    if (arguments.command == NULL) {
        const pid_t uid = getuid();
        struct passwd *pw = getpwuid(uid);
        if (pw == NULL) {
            fail("Error getting password entry for current user ID %d: %s\n", uid, strerror(errno));
        }
        if (pw->pw_shell == NULL) {
            fail("Command argument not provided and user's default shell undefined.");
        }
        arguments.command = pw->pw_shell;
    }

    if (arguments.binds != NULL) {
        const char* bind_path;
        uint bind_idx=0;
        while ((bind_path = arguments.binds[bind_idx++]) != NULL) {
            validate_directory(bind_path);
        }
    }

    const char* user_cache_path = get_user_cache_path();
    const char* app_base_path = auto_sprintf_stack("%s/%s", user_cache_path, APP_NAME);
    g_mount_base_path = auto_sprintf("%s/%s", app_base_path, arguments.mount_id);
    // mkdir_for_root(APP_BASE_DIR);
    // mkdir_for_root(g_mount_base_path);chocho
    mkdir_for_caller(app_base_path);
    mkdir_for_caller(g_mount_base_path);

    if (arguments.source_path == NULL) {
        arguments.source_path = auto_sprintf("%s/" EPHEMERAL_SOURCE_DIR_NAME, g_mount_base_path);
        g_source_is_ephemeral = true;
        mkdir_for_caller(arguments.source_path);
    }
    else {
        arguments.source_path = realpath(arguments.source_path, NULL);
    }

    if (g_verbose) {
        printf("Mount ID: %s\n",          arguments.mount_id);
        printf("Command: %s\n",           arguments.command);
        printf("Mount base path: %s\n",   g_mount_base_path);
        printf("Source path: %s", arguments.source_path);
        if (g_source_is_ephemeral) {
            puts(" (ephemeral)");
            printf("\nEphemeral source directory created for this sandbox: %s\n"
                   "It will be deleted automatically when this sandbox exits.\n"
                   "Copy it outside of the staging directory %s before the sandbox terminates to"
                   " preserve any filesystem mutations from within the sandbox.\n\n",
                   arguments.source_path, g_mount_base_path);
        }
        else puts("");
        if (arguments.binds != NULL) {
            printf("Additional bind mounts:");
            const char* bind = NULL;
            uint bind_idx=0;
            while ((bind = arguments.binds[bind_idx++]) != NULL) {
                bind = realpath(bind, NULL);
                printf(" %s", bind);
                free((void*)bind);
            }
            printf("\n");
        }
    }

    validate_directory(arguments.source_path);
    validate_directory(g_mount_base_path);

    const char* mount_name = auto_sprintf_stack(APP_NAME "-%s", arguments.mount_id);

    // Ensure mount/directory cleanup still happens on unexpected exit.
    atexit(cleanup);
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP,  signal_handler);
    signal(SIGQUIT, signal_handler);

    int status = create_sandbox(mount_name,
                                g_mount_base_path,
                                arguments.source_path,
                                arguments.binds,
                                arguments.command);

    if (g_verbose) {
        printf("Command exited with status: %d\n", WEXITSTATUS(status));
    }

    return status;
}