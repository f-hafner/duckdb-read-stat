#include "readstat.h"
#include <stdio.h>
#include <string.h>

int handle_metadata(readstat_metadata_t *metadata, void *ctx) {
    int *my_var_count = (int *)ctx;
    
    *my_var_count = readstat_get_var_count(metadata);

    return READSTAT_HANDLER_OK;
}

int handle_variable(int index, readstat_variable_t *variable, 
    const char *val_labels, void *ctx) {
    int *my_var_count = (int *)ctx;

    const char *var_name = readstat_variable_get_name(variable); 

    printf("%s", readstat_variable_get_name(variable));
    // this works, but the 'wage' string is still in the table header (column names)
    if (strcmp(var_name, "wage") == 0) {
        return READSTAT_HANDLER_SKIP_VARIABLE;
    } else if (index == *my_var_count - 1) {
        printf("\n");
    } else {
        printf("\t");
    }

    return READSTAT_HANDLER_OK;
}

int handle_value(int obs_index, readstat_variable_t *variable, readstat_value_t value, void *ctx) {
    int *my_var_count = (int *)ctx;
    int var_index = readstat_variable_get_index(variable);
    readstat_type_t type = readstat_value_type(value);
    if (!readstat_value_is_system_missing(value)) {
        if (type == READSTAT_TYPE_STRING) {
            printf("%s", readstat_string_value(value));
        } else if (type == READSTAT_TYPE_INT8) {
            printf("%hhd", readstat_int8_value(value));
        } else if (type == READSTAT_TYPE_INT16) {
            printf("%hd", readstat_int16_value(value));
        } else if (type == READSTAT_TYPE_INT32) {
            printf("%d", readstat_int32_value(value));
        } else if (type == READSTAT_TYPE_FLOAT) {
            printf("%f", readstat_float_value(value));
        } else if (type == READSTAT_TYPE_DOUBLE) {
            printf("%lf", readstat_double_value(value));
        }
    }
    if (var_index == *my_var_count - 1) {
        printf("\n");
    } else {
        printf("\t");
    }

    return READSTAT_HANDLER_OK;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    int my_var_count = 0;
    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = readstat_parser_init();
    readstat_set_metadata_handler(parser, &handle_metadata);
    readstat_set_variable_handler(parser, &handle_variable);
    readstat_set_value_handler(parser, &handle_value);
    readstat_set_row_limit(parser, 10);

    error = readstat_parse_dta(parser, argv[1], &my_var_count);

    readstat_parser_free(parser);

    if (error != READSTAT_OK) {
        printf("Error processing %s: %d\n", argv[1], error);
        return 1;
    }
    return 0;
}
