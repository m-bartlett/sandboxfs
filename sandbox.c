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
    {"/tmp",   "/tmp",     NULL,        MS_SLAVE|MS_BIND|MS_REC , NULL},
    {"/run",   "/run",     NULL,        MS_SLAVE|MS_BIND|MS_REC , NULL},
};
static const uint mount_template_max_size = sizeof(SANDBOX_MOUNT_PATH_TEMPLATE"/dev/shm");

// TODO: user bind mounts
// TODO: --force flag to make base dir?

int create_sandbox(const char* mount_name,
                   const char* mount_base_path,
                   const char* source_path,
                   const char* command) {
    validate_directory(mount_base_path);
    validate_directory(source_path);

    //malloc mount_point on heap so cleanup fubctions have access to it.
    g_mount_point = auto_sprintf(SANDBOX_MOUNT_PATH_TEMPLATE, mount_base_path, "");
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

    g_sandbox_pid = fork();

    if (g_sandbox_pid == -1) {
        fail("Failed to fork process: %s\n", strerror(errno));
    }
    else if (g_sandbox_pid == 0) {

        if (unshare(CLONE_NEWNS) != 0) {
            fail("Unshare failed: %s\n", strerror(errno));
        }

        mount_safe(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

        const uint max_path_size = (mount_template_max_size + strlen(mount_base_path));
        char path_buffer[max_path_size];

        for (uint i = 0; i < array_size(sandbox_mounts); ++i) {
            mount_t m = sandbox_mounts[i];
            snprintf(path_buffer, max_path_size,
                     SANDBOX_MOUNT_PATH_TEMPLATE, mount_base_path, m.dest);
            mkdir_safe(path_buffer);
            mount_safe(m.source, path_buffer, m.type, m.flags, (void*)m.data);
        }

        const char *pwd = getcwd(NULL, 0);
        chdir_safe(g_mount_point);
        chroot_safe(g_mount_point);
        chdir_safe(pwd); // Retain working directory within chroot

        // Hide overlay mount path from within itself
        mount_safe(NULL, (char*)mount_base_path, "tmpfs", MS_NOSUID|MS_NOEXEC, "mode=1755");
                                                                //TODO NULL options ^

        // Shed privileges before exec
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
    bool unmount_successful = false;

    if (g_sandbox_pid > 0) { // Parent cleanup
        kill(g_sandbox_pid, SIGTERM);

        if (g_mount_point != NULL) {
            if (umount2(g_mount_point, 0) == -1) {
                eprintf("Failed to unmount %s: %s\n", g_mount_point, strerror(errno));
            }
            else {
                unmount_successful = true;
                if (g_verbose) printf("Unmounted %s\n", g_mount_point);
            }
            free(g_mount_point);
        }
    }
    else { // child cleanup
        exit(EXIT_FAILURE);
    }

    return unmount_successful;
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
            fail("Running in a chroot but could not find " APP_NAME " mount, exitting.");
        }
        return true;
    }
    return false;
}