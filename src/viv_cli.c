#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "viv_layout.h"
#include "viv_mappable_functions.h"

#define LAST_OPTION {0, 0, 0, 0}

#define MACRO_FOR_EACH_OPTION(MACRO)            \
    MACRO("help", help, optional_argument, 0, 'h')                       \
    MACRO("list-config-options", list_config_options, optional_argument, 0, 0) \

#define GENERATE_OPTION_STRUCT(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL)  \
    {CLI_NAME, HAS_ARG, FLAG, VAL},

static bool handle_help(void) {
    printf(
        "Usage: vivarium [-h] [--list-config-options]\n"
        "\n"
        "-h, --help               Show help message and quit\n"
        "--list-config-options    List available layouts and keybinds\n"
        "\n"
    );

    return true;
}

#define PRINT_LAYOUT_HELP(LAYOUT_NAME, HELP, DIAGRAM)   \
    printf("  " #LAYOUT_NAME ": " HELP "\n" DIAGRAM "\n\n\n");

#define PRINT_MAPPABLE_HELP(FUNCTION_NAME, DOC, ...)    \
    printf("  " #FUNCTION_NAME ": " DOC "\n");

static bool handle_list_config_options(void) {
    printf(
        "# Layouts\n"
        "\n"
        "To use a layout, add it to the layout list in your config.toml:\n"
        "  [[layout]]\n"
        "  name = \"Split\"\n"
        "  layout = \"split\"  # layout name selected from list below\n"
        "\n"
        );
    printf("Available layout names:\n\n");
    MACRO_FOR_EACH_LAYOUT(PRINT_LAYOUT_HELP);

    printf(
        "# Actions\n"
        "\n"
        "To use an action in a keybind, add it to the keybind list in your config.toml:\n"
        "  [[keybind]]\n"
        "  keysym = \"Q\"\n"
        "  action = \"terminate\"  # action name selected from the list below\n"
        "\n"
        "Available keybinds:\n"
        "\n"
        );
    MACRO_FOR_EACH_MAPPABLE(PRINT_MAPPABLE_HELP);
    printf("\n");

    return true;
}

#define GENERATE_OPTION_HANDLER_LOOKUP(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL) \
    &handle_ ## FUNC_NAME,

static bool (*option_handlers[])(void) = {
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

        bool should_exit = false;
        switch (option) {
        case 0:  // indicates a long option
            should_exit = (*option_handlers[option_index])();
            break;
        case 'h':
            should_exit = handle_help();
            break;
        default:
            break;
        }

        if (should_exit) {
            // This option terminates vivarium
            exit(0);
        }
    }
}
