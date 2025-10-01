#include <stdbool.h>

#define ARGUMENT_LIST(F) \
    F('l', lower,   1, const char*, PATHS, "Comma-separated list of lower directory paths") \
    F('u', upper,   1, const char*, PATHS, "Comma-separated list of upper directory paths") \
    F('w', work,    1, const char*, PATH,  "Working directory path") \
    F('c', command, 1, const char*, PATH,  "Path to executable command") \
    F('L', list,    0, bool,        ,      "List mode (flag, defaults to false)") \
    F('v', verbose, 0, bool,        ,      "Display extra information")

#define STRUCT_ITEM(short, long, has_arg, type, arg_name, help) type long;
typedef struct { ARGUMENT_LIST(STRUCT_ITEM) } Arguments;
#undef STRUCT_ITEM

void parse_args(int argc, char *argv[], Arguments *arguments);