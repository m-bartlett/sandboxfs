#define _GNU_SOURCE

#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>
#include "util.h"

extern bool g_verbose;

#define PATH_MAX 4096
static char  g_mount_point[PATH_MAX] = {0};
static int   g_mount_done = 0;
static pid_t g_child_pid = -1;

static void mount_safe(const char* mount_name,
                      char* mount_path,
                      const char* mount_type,
                      unsigned long flags,
                      void* mounta_data)
{
    __label__ mount_fail;
    int result = mount(mount_name, mount_path, mount_type, 0, mounta_data);
    if (result != 0) goto mount_fail;
    if (flags!=0) {
        result = mount(mount_name, mount_path, mount_type, flags | MS_REMOUNT, mounta_data);
        if (result != 0) goto mount_fail;
    }
     if (g_verbose) {
        eprintf("%s mount at '%s' named '%s' succeeded.\n", mount_type, mount_path, mount_name);
    }
    return;

    mount_fail:
    fail("Mount at '%s' failed:\n"
         "  Name: %s\n"
         "  Type: %s\n"
         "  Data: %s\n"
         "  Error: %s\n",
         mount_path, mount_name, mount_type,
         mounta_data != NULL ? (char*)mounta_data : "",
         strerror(errno));
}

static void unshare_safe(int flags) {
    if (unshare(flags) != 0) {
        fail("Call to unshare failed: %s\n", strerror(errno));
    }
}


void mounts_destroy(void) {
    if (g_mount_done && g_mount_point[0] != '\0') {
        eprintf("Cleaning up: Unmounting %s\n", g_mount_point);

        // Kill child process if it's still running
        if (g_child_pid > 0) {
            kill(g_child_pid, SIGTERM);
            usleep(100000); // Give it 100ms to terminate
            kill(g_child_pid, SIGKILL); // Force kill if still alive
        }

        // Try to unmount - it might fail if we're in a chroot, that's fine
        umount(g_mount_point);

        // If we've done a chroot, we won't be able to directly unmount from original path
        // So we also try with the original path
        char cmd[PATH_MAX + 32];
        snprintf(cmd, sizeof(cmd), "umount %s 2>/dev/null", g_mount_point);
        system(cmd);
    }
}

/* MS_REMOUNT MS_BIND MS_SHARED MS_PRIVATE MS_SLAVE MS_UNBINDABLE MS_MOVE */

#define MOUNT_PATH_TEMPLATE "%s/mount"

#define MOUNT_LIST(F) \
F(proc,    proc,     "/proc",    0, "") \
F(sys,     sysfs,    "/sys",     0, "") \
F(tmp,     tmpfs,    "/tmp",     MS_PRIVATE|MS_STRICTATIME|MS_NODEV|MS_NOSUID, "mode=1777") \
F(udev,    devtmpfs, "/dev",     MS_PRIVATE|MS_NOSUID,                         "mode=0755") \
F(devpts,  devpts,   "/dev/pts", MS_PRIVATE|MS_NOSUID|MS_NOEXEC,               "mode=0620") \
F(shm,     tmpfs,    "/dev/shm", MS_PRIVATE|MS_NOSUID|MS_NOEXEC,               "mode=1777")

//TODO: --bind flag for verbatim mount paths

/*

    "--rbind --make-unbindable --make-private /tmp $VROOT_DIR_MOUNT/tmp"
    "proc proc $VROOT_DIR_MOUNT/proc"
    "sysfs sys $VROOT_DIR_MOUNT/sys"
    "tmp $VROOT_DIR_MOUNT/tmp -t tmpfs -o mode=1777,strictatime,nodev,nosuid"
    "udev $VROOT_DIR_MOUNT/dev -t devtmpfs -o mode=0755,nosuid"
    "devpts $VROOT_DIR_MOUNT/dev/pts -t devpts -o mode=0620,gid=5,nosuid,noexec"
    "shm $VROOT_DIR_MOUNT/dev/shm -t tmpfs -o mode=1777,nosuid,nodev"
    "--rbind --make-unbindable --make-private /run $VROOT_DIR_MOUNT/run"
    "--rbind --make-unbindable --make-private $HOME/Desktop $VROOT_DIR_MOUNT$HOME/Desktop"

*/

#define MOUNT_TEMPLATE_SIZE(mount_name, mount_type, mount_path, mount_flags, mount_data) \
    char mount_name##_[sizeof(MOUNT_PATH_TEMPLATE mount_path)];
static const uint mount_template_max_size = sizeof(union{MOUNT_LIST(MOUNT_TEMPLATE_SIZE)});
#undef MOUNT_TEMPLATE_SIZE

// TODO: --force flag to make base dir?

void create_sandbox_mounts(const char* mount_name,
                           const char* mount_base_path,
                           const char* source_path) {
    validate_directory(mount_base_path);
    validate_directory(source_path);
    const uint max_path_size = (mount_template_max_size + strlen(mount_base_path));
    char path_buffer[max_path_size];

    sprintf(path_buffer, "%s/work", mount_base_path);
    mkdir_for_caller(path_buffer);

    sprintf(path_buffer, MOUNT_PATH_TEMPLATE, mount_base_path);
    mkdir_for_caller(path_buffer);

    char* overlay_options = auto_sprintf("lowerdir=/,upperdir=%s,workdir=%s/work",
                                         source_path, mount_base_path);

    unshare_safe(CLONE_NEWNS);

    mount_safe(mount_name, path_buffer, "overlay",
               MS_SLAVE | MS_LAZYTIME | MS_NOATIME | MS_NODIRATIME | MS_REC,
               overlay_options);


    #define MOUNT_ENTRY(mount_name, mount_type, mount_path, mount_flags, mount_data) \
        snprintf(path_buffer, max_path_size, MOUNT_PATH_TEMPLATE mount_path, mount_base_path); \
        mkdir_safe(path_buffer); \
        mount_safe(#mount_name, path_buffer, #mount_type, mount_flags, mount_data);

    MOUNT_LIST(MOUNT_ENTRY);

    #undef MOUNT_ENTRY

}


/*
void list_sandboxfs_mounts(void) {
    FILE *mounts_file = fopen("/proc/mounts", "r");
    if (!mounts_file) {
        eprintf("Error: Could not open /proc/mounts: %s\n", strerror(errno));
        return;
    }

    printf("\n%-20s %-40s %-20s %-20s\n", "MOUNT POINT", "LAYERS", "CREATED BY", "CREATED AT");
    printf("%-20s %-40s %-20s %-20s\n", "--------------------", "----------------------------------------",
           "--------------------", "--------------------");

    char line[4096];
    int found = 0;

    while (fgets(line, sizeof(line), mounts_file)) {
        char fs_type[256] = {0};
        char mount_point[PATH_MAX] = {0};
        char options[4096] = {0};

        // Parse mount entry
        if (sscanf(line, "%*s %s %s %s", mount_point, fs_type, options) != 3) {
            continue;
        }

        // Check if it's an overlay mount with our identifier
        if (strcmp(fs_type, "overlay") == 0 && strstr(options, PROGRAM_NAME)) {
            found = 1;

            // Extract information from the mount options
            char *layers = "unknown";
            char *created_by = "unknown";
            char *created_at = "unknown";

            // Parse the options to get our custom data
            char *opt = strtok(options, ",");
            while (opt) {
                if (strncmp(opt, "lowerdir=", 9) == 0) {
                    layers = opt + 9;
                } else if (strncmp(opt, "creator=", 8) == 0) {
                    created_by = opt + 8;
                } else if (strncmp(opt, "created=", 8) == 0) {
                    created_at = opt + 8;
                }
                opt = strtok(NULL, ",");
            }

            // Shorten paths for display if needed
            char display_layers[41] = "...";
            if (strlen(layers) <= 40) {
                strncpy(display_layers, layers, 40);
                display_layers[40] = '\0';
            } else {
                strncpy(display_layers + 3, layers + strlen(layers) - 37, 37);
                display_layers[40] = '\0';
            }

            // Print the mount information
            printf("%-20s %-40s %-20s %-20s\n",
                   mount_point, display_layers, created_by, created_at);
        }
    }

    if (!found) {
        printf("No sandboxfs mounts found.\n");
    }

    fclose(mounts_file);
}*/