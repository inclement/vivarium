#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "viv_cli.h"
#include "viv_layout.h"
#include "viv_mappable_functions.h"
#include "viv_types.h"

#define ALL_OPTIONS_CONSUMED (-1)
#define OPTION_PARSE_FAILURE '?'

#define LAST_OPTION {0, 0, 0, 0}

#define MACRO_FOR_EACH_OPTION(MACRO)            \
    MACRO("help", help, no_argument, 0, 'h')                       \
    MACRO("list-config-options", list_config_options, no_argument, 0, 0) \
    MACRO("config", set_config_path, required_argument, 0, 0) \

#define GENERATE_OPTION_STRUCT(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL)  \
    {CLI_NAME, HAS_ARG, FLAG, VAL},

static bool handle_help(struct viv_args *args) {
    UNUSED(args);
    printf(
        "Usage: vivarium [-h] [--list-config-options] [--config]\n"
        "\n"
        "-h, --help               Show help message and quit\n"
        "--list-config-options    List available layouts and keybinds\n"
        "--config                 Path to config file to load, overrides normal config\n"
        "\n"
    );

    return true;
}

#define PRINT_LAYOUT_HELP(LAYOUT_NAME, HELP, DIAGRAM)   \
    printf("  " #LAYOUT_NAME ": " HELP "\n" DIAGRAM "\n\n\n");

#define PRINT_MAPPABLE_HELP(FUNCTION_NAME, DOC, ...)    \
    printf("  " #FUNCTION_NAME ": " DOC "\n");

static bool handle_list_config_options(struct viv_args *args) {
    UNUSED(args);
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

static bool handle_set_config_path(struct viv_args *args) {
    args->config_filen = optarg;
    return false;
}

#define GENERATE_OPTION_HANDLER_LOOKUP(CLI_NAME, FUNC_NAME, HAS_ARG, FLAG, VAL) \
    &handle_ ## FUNC_NAME,

static bool (*option_handlers[])(struct viv_args *args) = {
    MACRO_FOR_EACH_OPTION(GENERATE_OPTION_HANDLER_LOOKUP)
};

struct viv_args viv_cli_parse_args(int argc, char *argv[]) {
    int option_result;

    struct viv_args parsed_args = { 0 };

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            MACRO_FOR_EACH_OPTION(GENERATE_OPTION_STRUCT)
            LAST_OPTION,
        };
        option_result = getopt_long(argc, argv, "h", long_options, &option_index);

        if (option_result == ALL_OPTIONS_CONSUMED) {
            break;
        }

        bool should_exit = false;
        switch (option_result) {
        case 0:  // indicates a long option
            should_exit = (*option_handlers[option_index])(&parsed_args);
            break;
        case 'h':
            should_exit = handle_help(&parsed_args);
            break;
        case OPTION_PARSE_FAILURE:
            // In this case getopt_long has already printed an error message so we don't
            // need to add anything
            should_exit = true;
            break;
        default:
            UNREACHABLE();
        }

        if (should_exit) {
            // This option terminates vivarium
            exit(0);
        }
    }

    return parsed_args;
}
