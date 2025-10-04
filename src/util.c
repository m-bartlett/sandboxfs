#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>


void validate_directory(const char* path) {
    if (!path || path[0] == '\0') {
        fail("Empty string was given for directory path");
    }
    struct stat s = stat_safe(path);
    if (!S_ISDIR(s.st_mode)) {
        fail("Path '%s' is not a directory", path);
    }
}

bool read_file_lines(const char* filename, bool(*callback)(char*, uint)) {
    int fd = file_open_safe(filename, O_RDONLY);
    int line_size=0, read_size=0;
    char c = 0;
    bool callback_exit = false;
    do {
        if (c == '\n') {
            lseek(fd, -line_size, SEEK_CUR);
            char line[line_size];
            line[--line_size] = 0;
            read(fd, line, line_size);
            if (callback(line, line_size)) {
                callback_exit = true;
                break;
            }
            line_size=0;
            read(fd, line, 1);
        }
        read_size = read(fd, &c, 1);
        line_size += read_size;
    } while (read_size > 0);
    file_close_safe(fd);
    return callback_exit;

}


inline static bool is_dot(const char* path) {
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

int remove_directory_recursive(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat s;
    int result = 0;

    if ((dir = opendir(path)) == NULL) {
        eprintf("Failed to open directory %s: %s\n", path, strerror(errno));
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (is_dot(entry->d_name)) continue;
        char* child_path = string_concat(path, "/", entry->d_name);

        if (lstat(child_path, &s) == -1) {
            eprintf("Failed to stat %s: %s\n", child_path, strerror(errno));
            goto entry_error;
        }

        if (S_ISDIR(s.st_mode)) {
            result = remove_directory_recursive(child_path);
        }
        else if (unlink(child_path) == -1) {
            eprintf("Failed to remove %s: %s\n", child_path, strerror(errno));
            goto entry_error;
        }

        continue;

    entry_error:
        result = -1;
        break;
    }

    closedir(dir);

    if (result == 0) {
        if (rmdir(path) == -1) {
            eprintf("Error removing directory %s: %s\n", path, strerror(errno));
            result = -1;
        }
    }

    return result;
}