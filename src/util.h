#ifndef UTIL_H
#define UTIL_H

#ifndef _GNU_SOURCE
#define _GNU_GNU_SOURCE
#endif

#include <alloca.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>


#define APP_NAME "sandboxfs"

#ifndef APP_BASE_DIR
#define APP_BASE_DIR "/var/lib/" APP_NAME
#endif

#define EPHEMERAL_SOURCE_DIR_NAME "delta"

#define auto_sprintf_stack(format, ...) ({ \
    const uint formatted_size = snprintf(NULL, 0, format __VA_OPT__(,) __VA_ARGS__); \
    char* pointer = alloca(formatted_size); \
    snprintf(pointer, formatted_size+1, format __VA_OPT__(,) __VA_ARGS__); \
    pointer; \
})

#define auto_sprintf(format, ...) ({ \
    const uint formatted_size = snprintf(NULL, 0, format __VA_OPT__(,) __VA_ARGS__); \
    char* pointer = malloc(formatted_size); \
    snprintf(pointer, formatted_size+1, format __VA_OPT__(,) __VA_ARGS__); \
    pointer; \
})

#define string_concat(a, glue, b) ({ \
    size_t __path_size = strlen(a) + strlen(b) + strlen(glue) + 1; \
    char *__path = alloca(__path_size); \
    snprintf(__path, __path_size, "%s" glue "%s", a, b); \
    __path; \
})


#define eprintf(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)

#define fail(format, ...) eprintf(format __VA_OPT__(,) __VA_ARGS__); exit(EXIT_FAILURE);

#define null2str(s) (s == NULL ? "(null)" : (char*)s)

#define array_size(a) (sizeof(a)/sizeof(a[0]))

inline static const char* current_timestamp() {
    #define TIMESTAMPFMT "%Y-%m-%d-%H-%M-%S"
    #define TIMESTAMPFMTSZ 20
    static char timestamp[TIMESTAMPFMTSZ]={0};
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, TIMESTAMPFMTSZ, TIMESTAMPFMT, tm_info);
    return timestamp;
}

inline static void seteuid_safe(const uid_t euid) {
    if (seteuid(euid) != 0) {
        fail("Setting uid %d failed: %s\n", euid, strerror(errno));
    }
}

inline static void setegid_safe(const gid_t egid) {
    if (setegid(egid) != 0) {
        fail("Setting uid %d failed: %s\n", egid, strerror(errno));
    }
}

inline static void set_user_id_safe(const uid_t _id) {
    seteuid_safe(_id);
    setegid_safe(_id);
}

inline static struct stat stat_safe(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fail("Failed to stat '%s': %s\n", path, strerror(errno));
    }
    return st;
}

inline static void mkdir_safe(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            fail("Failed to create directory %s: %s\n", path, strerror(errno));
        }
    }
}

inline static void rmdir_safe(const char* path) {
    if (rmdir(path) == -1) {
        eprintf("Removing dir %s failed: %s\n", path, strerror(errno));
    }
}

inline static void chown_safe(const char* path, const uid_t uid, const gid_t gid) {
    if (chown(path, uid, gid) != 0) {
        fail("Failed to chown %s to %d:%d:\n%s", path, uid, gid, strerror(errno));
    }
}

inline static void mkdir_for_user(const char* path, const uid_t uid, const gid_t gid) {
    mkdir_safe(path);
    chown_safe(path, uid, gid);
}

inline static void mkdir_for_caller(const char* path) {
    mkdir_for_user(path, getuid(), getgid());
}

inline static void mkdir_for_root(const char* path) {
    mkdir_for_user(path, 0, 0);
}

inline static int file_open_safe(const char* filename, int flags) {
    int fd = open(filename, flags);
    if (fd == -1) {
        fail("Could not open file %s:\n%s", filename, strerror(errno));
    }
    return fd;
}

inline static void file_close_safe(int fd) {
    if (close(fd) == -1) {
        eprintf("Close file failed:\n%s", strerror(errno)); //No need to exit program
    }
}

inline static pid_t fork_safe() {
    pid_t pid = fork();
    if (pid == -1) fail("Failed to fork process: %s\n", strerror(errno));
    return pid;
}

inline static void chdir_safe(const char* path) {
    if (chdir(path) != 0) {
        eprintf("Failed changing directory to %s: %s\n", path, strerror(errno));
    }
}

inline static void pivot_root_safe(const char* new_root) {
    if (syscall(SYS_pivot_root, new_root, new_root) != 0) {
        fail("Root pivot to %s failed: %s\n", new_root, strerror(errno));
    }
}

void validate_directory(const char* path);

bool read_file_lines(const char* filename, bool(*callback)(char*, uint));

int remove_directory_recursive(const char *path);

// Capability management functions
void request_cap_sys_admin();
void drop_all_capabilities();


#endif