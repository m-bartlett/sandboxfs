#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include "args.h"

#define eprintf(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)


#define OPSTRING_SEP_0
#define OPSTRING_SEP_1 ':',
#define OPSTRING_SEP_2 OPSTRING_SEP_1
#define OPTSTRING_ITEM(short, long, has_arg, type, arg_name, help) short, OPSTRING_SEP_##has_arg
const char optstring[] = { ARGUMENT_LIST(OPTSTRING_ITEM) 'h', '\0' };
#undef OPTSTRING_ITEM
#undef OPSTRING_SEP_0
#undef OPSTRING_SEP_1
#undef OPSTRING_SEP_2

#define LONGOPT_ITEM(short, long, has_arg, type, arg_name, help) { #long, has_arg, NULL, short },
static const struct option long_options[] = {
    ARGUMENT_LIST(LONGOPT_ITEM)
    {"help", 0, NULL , 'h' },
    {NULL, 0, 0, 0}
};
#undef LONGOPT_ITEM


#define HELP_ITEM(short, long, has_arg, type, arg_name, help) \
    eprintf("  -%c, --%-15s %s\n", short, #long " " #arg_name, help);
static inline void print_help(const char *prog) {
    eprintf("Usage: %s\n\n", prog);
    eprintf("Arguments:\n");
    eprintf("  -%c, --%-15s %s\n", 'h', "help", "Show this help message and exit.");
    ARGUMENT_LIST(HELP_ITEM)
}
#undef HELP_ITEM

void parse_args(int argc, char *argv[], Arguments *arguments) {
    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);

            #define OPTARG_0 true
            #define OPTARG_1 optarg
            #define OPTARG_2 OPTARG_1
            #define PARSE_CASE(short, long, has_arg, type, arg_name, help) \
                case short : arguments->long = OPTARG_##has_arg; break;
                ARGUMENT_LIST(PARSE_CASE)
            #undef OPTARG_0
            #undef OPTARG_1
            #undef OPTARG_2
            #undef PARSE_CASE

            default:
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }
}