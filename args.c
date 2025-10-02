#include "util.h"
#include "args.h"

#define ARG_NONE     no_argument
#define ARG_REQUIRED required_argument
#define ARG_OPTIONAL optional_argument
#define ARG_MULTIPLE required_argument



#define OPTSYM_ARG_NONE
#define OPTSYM_ARG_REQUIRED ':',
#define OPTSYM_ARG_OPTIONAL OPTSYM_ARG_REQUIRED
#define OPTSYM_ARG_MULTIPLE OPTSYM_ARG_REQUIRED

#define OPTSTRING_ITEM(short, long, var, has_arg, type, arg, help) short, OPTSYM_##has_arg
const char optstring[] = { ARGUMENT_LIST(OPTSTRING_ITEM) 'h', '\0' };

#undef OPTSTRING_ITEM
#undef OPTSYM_ARG_NONE
#undef OPTSYM_ARG_REQUIRED
#undef OPTSYM_ARG_OPTIONAL
#undef OPTSYM_ARG_MULTIPLE



#define LONGOPT_ITEM(short, long, var, has_arg, type, arg, help) \
    { long, has_arg, NULL, short },

static const struct option long_options[] = {
    ARGUMENT_LIST(LONGOPT_ITEM)
    {"help", 0, NULL , 'h' },
    {NULL, 0, 0, 0}
};

#undef LONGOPT_ITEM



#define HELP_SPACE "20"
#define HELP_ITEM(short, long, var, has_arg, type, arg, help) \
    eprintf("  -%c, --%-"HELP_SPACE"s %s\n", short, long " " #arg, help);

static inline __attribute__((__noreturn__))
void print_help(const char *prog) {
    eprintf("Usage: %s\n\n", prog);
    eprintf("Arguments:\n");
    eprintf("  -%c, --%-"HELP_SPACE"s %s\n", 'h', "help", "Show this help message and exit.");
    ARGUMENT_LIST(HELP_ITEM);
    exit(EXIT_FAILURE);
}

#undef HELP_ITEM
#undef HELP_SPACE



static const char** parse_multiple_arg_values(int argc, char *argv[]) {
    uint args=0, arg_start = --optind;
    while (optind < argc && argv[optind][0] != '-') {
        optind++;
        args++;
    }
    if (args == 0) return NULL;
    const char** arglist = malloc(sizeof(char*[args+1]));
    for (uint i = 0; i < args; ++i) arglist[i] = argv[arg_start++];
    arglist[args] = NULL;
    return arglist;
}



#define CASE_ARG_NONE true
#define CASE_ARG_REQUIRED optarg
#define CASE_ARG_OPTIONAL optarg
#define CASE_ARG_MULTIPLE parse_multiple_arg_values(argc, argv)

#define PARSE_CASE(short, long, var, has_arg, type, arg, help) \
    case short : arguments->var = CASE_##has_arg; break;

void parse_args(int argc, char *argv[], Arguments *arguments) {
    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_help(argv[0]);

            ARGUMENT_LIST(PARSE_CASE)

            default:
            case '?':
                print_help(argv[0]);
        }
    }
}

#undef CASE_ARG_NONE
#undef CASE_ARG_REQUIRED
#undef CASE_ARG_OPTIONAL
#undef CASE_ARG_MULTIPLE
#undef PARSE_CASE