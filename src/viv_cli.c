#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define LAST_OPTION {0, 0, 0, 0}

#define MACRO_FOR_EACH_OPTION(MACRO)            \
    MACRO("help", help, optional_argument, 0, 'h')                       \
    MACRO("list-config-options", list_config_options, optional_argument, 0, 0) \

#define GENERATE_OPTION_STRUCT(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL)  \
    {CLI_NAME, HAS_ARG, FLAG, VAL},

static void handle_help(void) {
    printf(
        "Usage: vivarium [-h] [-v]\n"
        "\n"
        "-h, --help               Show help message and quit\n"
        "--list-config-options    List available layouts and keybinds\n"
        "\n"
    );
}

static void handle_list_config_options(void) {
}

#define GENERATE_OPTION_HANDLER_LOOKUP(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL) \
    &handle_ ## FUNC_NAME,

static void (*option_handlers[])(void) = {
    MACRO_FOR_EACH_OPTION(GENERATE_OPTION_HANDLER_LOOKUP)
};

void viv_cli_parse_args(int argc, char *argv[]) {
    int option;
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            MACRO_FOR_EACH_OPTION(GENERATE_OPTION_STRUCT)
            LAST_OPTION,
        };
        option = getopt_long(argc, argv, "h", long_options, &option_index);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 0:  // indicates a long option
            (*option_handlers[option_index])();
            break;
        case 'h':
            handle_help();
            break;
        default:
            break;
        }
    }
    exit(0);
}
