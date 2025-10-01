#include <stdbool.h>

#define SANDBOX_MOUNT_PATH_TEMPLATE "%s/mount%s"

typedef struct { const char* source;
                 const char* dest;
                 const char* type;
                 int flags;
                 const char* data; } mount_t;

int create_sandbox(const char* mount_name,
                   const char* mount_base_path,
                   const char* source_path,
                   const char* command);

bool detect_sandbox();
bool cleanup_sandbox();