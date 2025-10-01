#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include "args.h"
#include "util.h"
#include "mount.h"

bool g_verbose;

static Arguments arguments = { NULL };

void signal_handler(int sig) {
    fprintf(stderr, "\nReceived signal %d, cleaning up...\n", sig);
    // cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}


int main(int argc, char *argv[]) {
    bool source_is_ephemeral = false;

    if (geteuid() != 0) {
        fail("ERROR: " APP_NAME
             " does not have elevated privileges (required for mount operations).\n"
             "Executable should be owned by root with setuid bit enabed.\n")
    }

    parse_args(argc, argv, &arguments);
    g_verbose = arguments.verbose;

    if (arguments.mount_id == NULL) {
        arguments.mount_id = current_timestamp();
    }

    const char* mount_base_path = auto_sprintf(APP_BASE_DIR "/%s", arguments.mount_id);
    mkdir_for_root(APP_BASE_DIR);
    mkdir_for_root(mount_base_path);

    if (arguments.source_path == NULL) {
        arguments.source_path = auto_sprintf("%s/delta", mount_base_path);
        source_is_ephemeral = true;
        mkdir_for_caller(arguments.source_path);
    }

    if (g_verbose) {
        eprintf("Mount ID: %s\n",          arguments.mount_id);
        eprintf("Mount base path: %s\n",   mount_base_path);
        eprintf("Source path: %s%s\n",
                arguments.source_path, (source_is_ephemeral ? " (ephemeral)" : ""));
        eprintf("Command: %s\n",           arguments.command);
        eprintf("List mode: %s\n",         arguments.list ? "true" : "false");
        eprintf("uid:%d gid:%d euid:%d egid:%d\n", getuid(), getgid(), geteuid(), getegid());
    }

    validate_directory(arguments.source_path);
    validate_directory(mount_base_path);

    const char* mount_name = auto_sprintf(APP_NAME "-%s", arguments.mount_id);
    create_sandbox_mounts(mount_name, mount_base_path, arguments.source_path);

    // atexit(cleanup);

    // signal(SIGINT,  signal_handler);
    // signal(SIGTERM, signal_handler);
    // signal(SIGHUP,  signal_handler);
    // signal(SIGQUIT, signal_handler);

    // uid_t real_uid = getuid();
    // uid_t effective_uid = geteuid();



    /*pid_t pid = fork();

    if (pid == -1) {
        fprintf(stderr, "Failed to fork process: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    else if (pid == 0) {
        printf("Creating new mount namespace\n");
        if (unshare(CLONE_NEWNS) != 0) {
            fprintf(stderr, "Failed to create new namespace: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Make all mounts private to avoid affecting the parent namespace
        if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
            fprintf(stderr, "Failed to make mounts private: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Setup the chroot environment with essential mounts
        if (prepare_chroot_env(mount_point) != 0) {
            fprintf(stderr, "Failed to prepare chroot environment\n");
            exit(EXIT_FAILURE);
        }

        printf("Changing root to: %s\n", mount_point);

        // Change to the new root directory
        if (chdir(mount_point) != 0) {
            fprintf(stderr, "Failed to change directory to %s: %s\n", mount_point, strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Change the root
        if (chroot(mount_point) != 0) {
            fprintf(stderr, "Failed to change root: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Change to root directory inside the new root
        if (chdir("/") != 0) {
            fprintf(stderr, "Failed to change directory inside chroot: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        printf("Executing %s in the new namespace\n", command);

        // Prepare arguments for the command
        char *exec_args[64] = {command, NULL}; // Start with just the command
        int arg_count = 1;

        // Add additional arguments if specified
        if (command_args) {
            char *arg_copy = strdup(command_args);
            char *arg = strtok(arg_copy, " ");
            while (arg != NULL && arg_count < 63) {
                exec_args[arg_count++] = arg;
                arg = strtok(NULL, " ");
            }
            exec_args[arg_count] = NULL;
        }

        // Drop privileges back to the original user if running setuid
        if (real_uid != effective_uid) {
            printf("Dropping privileges from UID %d to UID %d\n", effective_uid, real_uid);
            if (setuid(real_uid) != 0) {
                fprintf(stderr, "Warning: Failed to drop privileges: %s\n", strerror(errno));
            }
        }

        // Execute the command
        execvp(command, exec_args);

        // If we get here, exec failed
        fprintf(stderr, "Failed to execute %s: %s\n", command, strerror(errno));
        exit(EXIT_FAILURE);
    }
    else {
        // Parent process
        g_child_pid = pid;

        // Wait for child to finish
        int status;
        waitpid(pid, &status, 0);

        printf("Command exited with status: %d\n", WEXITSTATUS(status));

        // Cleanup happens automatically through atexit handler
        return WEXITSTATUS(status);
    }
*/
    return EXIT_SUCCESS;
}