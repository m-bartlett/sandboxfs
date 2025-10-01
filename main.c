#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "args.h"

static Arguments arguments = { NULL };

int main(int argc, char *argv[]) {
    parse_args(argc, argv, &arguments);
    if (arguments.verbose) {
        printf("Parsed command line arguments:\n");
        printf("Lower directories: %s\n", arguments.lower);
        printf("Upper directories: %s\n", arguments.upper);
        printf("Work directory: %s\n",    arguments.work);
        printf("Command: %s\n",           arguments.command);
        printf("List mode: %s\n",         arguments.list ? "true" : "false");
        printf("Verbose: %s\n",           arguments.verbose ? "true" : "false");
    }
    return EXIT_SUCCESS;
}