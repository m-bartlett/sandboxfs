#include <stdbool.h>
#include <getopt.h>

/* short_flag, long_flag, var_name, has_arg, arg_type, arg_val_name, help */
#define ARGUMENT_LIST(F) \
F('i', "id", mount_id, 1, const char*, MOUNT_ID,\
  "Mount ID (defaults to current timestamp)") \
\
F('s', "source", source_path, 1, const char*, PATH,\
  "Path to directory to overlay on top of root filesystem (defaults to ephemeral temporary dir)") \
\
F('c', "cmd", command, 1, const char*, PATH,\
  "Path to executable command (Defaults to user's preferred shell).") \
\
F('v', "verbose", verbose, 0, bool, , "Display extra information")


#define STRUCT_ITEM(short, long, var, has_arg, type, arg, help) type var;
typedef struct { ARGUMENT_LIST(STRUCT_ITEM) } Arguments;
#undef STRUCT_ITEM

void parse_args(int argc, char *argv[], Arguments *arguments);