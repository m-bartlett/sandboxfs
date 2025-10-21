#define _GNU_SOURCE
#include "sandbox.h"
#include "util.h"

#include <sched.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <wordexp.h>

extern bool g_verbose;

static char* g_mount_point = NULL; // Keep ref to mountpoint for cleanup
static pid_t g_sandbox_pid = -1; // Keep ref to child PID for cleanup


static void mount_safe(const char*   mount_source,
                       char*         mount_path,
                       const char*   mount_type,
                       unsigned long flags,
                       void*         mounta_data);

static const mount_t sandbox_mounts[] = {
    {"proc",   "/proc",    "proc",      0,                        NULL},
    {"sys",    "/sys",     "sysfs",     0,                        NULL},
    {"dev",    "/dev",     "devtmpfs",  MS_NOSUID,                "mode=0755"},
    {"devpts", "/dev/pts", "devpts",    MS_NOSUID|MS_NOEXEC,      "mode=0620"},
    {"shm",    "/dev/shm", "tmpfs",     MS_NOSUID|MS_NOEXEC,      "mode=1777"},
    {"/run",   "/run",     NULL,        MS_SLAVE|MS_BIND|MS_REC , NULL},

#ifdef NO_BIND_TMP
    {"tmp",   "/tmp",     "tmpfs",      MS_NOSUID|MS_NOEXEC|MS_NODEV|MS_STRICTATIME, "mode=1777"},
#else
    {"/tmp",   "/tmp",     NULL,        MS_SLAVE|MS_BIND|MS_REC , NULL},
#endif

};
static const uint mount_template_max_size = sizeof(SANDBOX_MOUNT_PATH_TEMPLATE"/dev/shm");


int create_sandbox(const char*  mount_name,
                   const char*  mount_base_path,
                   const char*  source_path,
                   const char** bind_paths,
                   const char*  command) {
    validate_directory(mount_base_path);
    validate_directory(source_path);

    //malloc mount_point on heap so cleanup functions have access to it.
    g_mount_point = auto_sprintf(SANDBOX_MOUNT_PATH_TEMPLATE, mount_base_path, "");

    // Save real UID/GID before forking
    uid_t uid = getuid();
    gid_t gid = getgid();

    // Create a pipe for synchronization between parent and child
    int sync_pipe[2];
    if (pipe(sync_pipe) != 0) {
        fail("Failed to create sync pipe: %s\n", strerror(errno));
    }

    g_sandbox_pid = fork();

    if (g_sandbox_pid == -1) {
        fail("Failed to fork process: %s\n", strerror(errno));
    }
    else if (g_sandbox_pid == 0) {
        // Child process
        close(sync_pipe[1]); // Close write end

        // First, unshare only the user namespace
        if (unshare(CLONE_NEWUSER) != 0) {
            fail("User namespace unshare failed: %s\n", strerror(errno));
        }

        // Read UID/GID from parent
        uid_t parent_uid, parent_gid;
        if (read(sync_pipe[0], &parent_uid, sizeof(parent_uid)) != sizeof(parent_uid)) {
            fail("Failed to read parent UID: %s\n", strerror(errno));
        }
        if (read(sync_pipe[0], &parent_gid, sizeof(parent_gid)) != sizeof(parent_gid)) {
            fail("Failed to read parent GID: %s\n", strerror(errno));
        }
        close(sync_pipe[0]);

        // Write our own uid/gid mappings
        int fd = open("/proc/self/uid_map", O_WRONLY);
        if (fd < 0) {
            fail("Failed to open /proc/self/uid_map: %s\n", strerror(errno));
        }
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "0 %d 1\n", parent_uid);
        if (write(fd, buf, len) < 0) {
            fail("Failed to write uid_map: %s\n", strerror(errno));
        }
        close(fd);

        // Disable setgroups
        fd = open("/proc/self/setgroups", O_WRONLY);
        if (fd >= 0) {
            write(fd, "deny", 4);
            close(fd);
        }

        // Write gid mapping
        fd = open("/proc/self/gid_map", O_WRONLY);
        if (fd < 0) {
            fail("Failed to open /proc/self/gid_map: %s\n", strerror(errno));
        }
        len = snprintf(buf, sizeof(buf), "0 %d 1\n", parent_gid);
        if (write(fd, buf, len) < 0) {
            fail("Failed to write gid_map: %s\n", strerror(errno));
        }
        close(fd);

        // Debug: check UID/GID after mapping
        if (g_verbose) {
            printf("After uid/gid mapping: UID=%d, GID=%d\n", getuid(), getgid());
        }

        // Now unshare the mount namespace
        // This is required for unprivileged overlay mounts
        if (unshare(CLONE_NEWNS) != 0) {
            fail("Mount namespace unshare failed: %s\n", strerror(errno));
        }

        // Request CAP_SYS_ADMIN capability explicitly for mount operations
        // In a user namespace where we're mapped to root, we have all capabilities,
        // but we explicitly request it to satisfy the requirement
        request_cap_sys_admin();

        const char* mount_work_dir = auto_sprintf_stack("%s/work", mount_base_path);
        const char* overlay_options = auto_sprintf_stack("lowerdir=/,upperdir=%s,workdir=%s",
                                                          source_path, mount_work_dir);
        mkdir_for_caller(g_mount_point);
        mkdir_for_caller(mount_work_dir);
        mount_safe(mount_name,
                   g_mount_point,
                   "overlay",
                   MS_LAZYTIME | MS_NOATIME | MS_NODIRATIME,
                   (void*)overlay_options);


        const uint max_path_size = (mount_template_max_size + strlen(mount_base_path));
        char path_buffer[max_path_size];

        for (uint i = 0; i < array_size(sandbox_mounts); ++i) {
            mount_t m = sandbox_mounts[i];
            snprintf(path_buffer, max_path_size,
                     SANDBOX_MOUNT_PATH_TEMPLATE, mount_base_path, m.dest);
            mkdir_safe(path_buffer);
            mount_safe(m.source, path_buffer, m.type, m.flags, (void*)m.data);
        }

        if (bind_paths != NULL) {
            const char* bind_path;
            uint bind_idx=0;
            while ((bind_path = bind_paths[bind_idx++]) != NULL) {
                char* absolute_path = realpath(bind_path, NULL);
                char* sandboxed_path = string_concat(g_mount_point, "", absolute_path);
                mount_safe(absolute_path, sandboxed_path, NULL, MS_SLAVE|MS_BIND|MS_REC, NULL);
                free(absolute_path);
            }
        }

        const char *pwd = getcwd(NULL, 0);
        chdir_safe(g_mount_point);
        pivot_root_safe(g_mount_point);
        chdir_safe(pwd); // Retain working directory within the new root

        // Hide overlay mount path from within itself
        mount_safe(NULL, (char*)mount_base_path, "tmpfs", MS_NOSUID|MS_NOEXEC, "mode=1755");
                                                                //TODO NULL options ^
        // Drop all capabilities and shed privileges before exec
        drop_all_capabilities();
        seteuid_safe(getuid());
        setegid_safe(getgid());

        wordexp_t command_words;
        if (wordexp(command, &command_words, 0) != 0) {
            fail("Error parsing provided command: %s\n", strerror(errno));
        }

        execvp(command_words.we_wordv[0], command_words.we_wordv);

        wordfree(&command_words);
        fail("Exec failed: %s\n", strerror(errno));
    }
    else {
        // Parent process
        close(sync_pipe[0]); // Close read end

        // Send UID/GID to child so it can write its own mappings
        if (write(sync_pipe[1], &uid, sizeof(uid)) != sizeof(uid)) {
            fail("Failed to send UID to child: %s\n", strerror(errno));
        }
        if (write(sync_pipe[1], &gid, sizeof(gid)) != sizeof(gid)) {
            fail("Failed to send GID to child: %s\n", strerror(errno));
        }
        close(sync_pipe[1]);

        if (g_verbose) {
            printf("Sandbox PID: %d\n", g_sandbox_pid);
        }
        int status;
        waitpid(g_sandbox_pid, &status, 0);
        return status;
    }
    return -1;
}


bool cleanup_sandbox() {
    bool cleanup_success = true;

    if (g_sandbox_pid > 0) { // Parent cleanup
        kill(g_sandbox_pid, SIGTERM);

        if (g_mount_point != NULL) {
            // mount shouldn't be visible to parent, but doesn't hurt to make sure it's unmounted
            umount2(g_mount_point, 0);
            free(g_mount_point);
        }
    }
    else { // child cleanup
        // This block is only reached if an error prevents getting to the exec call
        free(g_mount_point);
        exit(EXIT_FAILURE);
    }

    return cleanup_success;
}


static void mount_safe(const char*   mount_source,
                       char*         mount_path,
                       const char*   mount_type,
                       unsigned long flags,
                       void*         mounta_data) {
    if (mount(mount_source, mount_path, mount_type, flags, mounta_data) != 0) {
        fail("Mount at '%s' failed:\n"
             "   Source: %s\n"
             "   Type:   %s\n"
             "   Data:   %s\n"
             "   Error:  %s\n",
             mount_path,
             null2str(mount_source),
             null2str(mount_type),
             null2str(mounta_data),
             strerror(errno));
    }
    else if (g_verbose) {
        printf("Created %s mount '%s' at %s\n",
                null2str(mount_type), null2str(mount_source), mount_path);
    }
}


static bool detect_sandbox_mount_line(char* mount_line, uint line_size) {
    char mount_source[line_size],
         mount_point[line_size],
         mount_type[line_size],
         mount_options[line_size];

    if (
        sscanf(mount_line, "%s %s %s %s",
               mount_source, mount_point, mount_type, mount_options) == 4 &&
        strcmp(mount_point, "/") == 0 &&
        strstr(mount_source, APP_NAME) != NULL
    ) {
        char *opt = strtok(mount_options, ",");
        char* sub;
        char* base = NULL;
        uint base_size = 0;
        char* source = NULL;
        uint source_size = 0;

        while (opt) {
            if ((sub=strstr(opt, "upperdir=")) != NULL) {
                source = strchr(sub, '=')+1;
                source_size = strrchr(opt, '\0') - source;
            }

            else if ((sub = strstr(opt, "workdir=")) != NULL) {
                base = strchr(sub, '=')+1;
                base_size = strrchr(opt, '/') - base;
            }

            opt = strtok(NULL, ",");
        }

        printf("sandbox_name=\"%s\"\n", mount_source);
        printf("sandbox_base=\"%.*s\"\n", base_size, base);
        printf("sandbox_source=\"%.*s\"\n", source_size, source);
        return true;
    }
    return false;
}


bool detect_sandbox() {
    struct stat s = stat_safe("/");
    if (s.st_ino != 2) {
        bool sandbox_mount_found = read_file_lines("/proc/mounts", detect_sandbox_mount_line);
        if (!sandbox_mount_found) {
            fail("Root mount is pivoted but could not find " APP_NAME " mount, exitting.");
        }
        return true;
    }
    return false;
}