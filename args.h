#include <stdbool.h>
#include <getopt.h>

/* short_flag, long_flag, var_name, has_arg, arg_type, arg_val_name, help */
#define ARGUMENT_LIST(F) \
F('i', "id", mount_id, ARG_REQUIRED, const char*, MOUNT_ID,\
  "Mount ID (defaults to current timestamp)") \
\
F('s', "source", source_path, ARG_REQUIRED, const char*, PATH,\
  "Path to overlay onto sandbox filesystem. Defaults to an ephemeral temporary dir.") \
\
F('c', "cmd", command, ARG_REQUIRED, const char*, COMMAND_STRING ,\
  "Executable command string (will be word expanded). Defaults to user's preferred shell.") \
\
F('b', "bind", binds, ARG_MULTIPLE, const char**, PATH [PATH ...],\
  "Paths to bind mount in the sandbox (i.e. changes to these occur to the actual rootfs).") \
\
F('v', "verbose", verbose, ARG_NONE, bool, , "Display extra information")


#define STRUCT_ITEM(short, long, var, has_arg, type, arg, help) type var;
typedef struct { ARGUMENT_LIST(STRUCT_ITEM) } Arguments;
#undef STRUCT_ITEM

void parse_args(int argc, char *argv[], Arguments *arguments);