#include <string.h>
#if defined(_MSC_VER)

#define strncasecmp _strnicmp
#define strcasecmp _stricmp

#else
#include <strings.h>
#endif

#include "duckdb.h"
#include "duckdb_extension.h"

#include "duckdb_read_stat.h"

DUCKDB_EXTENSION_EXTERN

int duckdb_read_stat_ends_with(const char *string, const char *suffix)
{
    if (string == NULL || suffix == NULL)
        return false;
    size_t lenstr = strlen(string);
    size_t lensuffix = strlen(suffix);

    return lensuffix > lenstr ? false : strncasecmp(string + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int duckdb_read_stat_bind_handle_metadata(readstat_metadata_t *metadata, void *ctx)
{
    duckdb_read_stat_bind_data *context = (duckdb_read_stat_bind_data *)ctx;
    context->cardinality = readstat_get_row_count(metadata);
    return READSTAT_HANDLER_OK;
}

int duckdb_read_stat_bind_handle_variable(int index, readstat_variable_t *variable,
                                          const char *val_labels, void *ctx)
{
    duckdb_read_stat_bind_data *context = (duckdb_read_stat_bind_data *)ctx;
    const char *name = readstat_variable_get_name(variable);
    readstat_type_t type = readstat_variable_get_type(variable);
    const char *format = readstat_variable_get_format(variable);
    duckdb_logical_type logical_type = NULL;

    switch (type)
    {
    case READSTAT_TYPE_STRING:
    case READSTAT_TYPE_STRING_REF:
        logical_type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
        break;
    case READSTAT_TYPE_INT8:
        logical_type = duckdb_create_logical_type(DUCKDB_TYPE_TINYINT);
        break;
    case READSTAT_TYPE_INT16:
        logical_type = duckdb_create_logical_type(DUCKDB_TYPE_SMALLINT);
        break;
    case READSTAT_TYPE_INT32:
        logical_type = duckdb_create_logical_type(DUCKDB_TYPE_INTEGER);
        break;
    case READSTAT_TYPE_FLOAT:
        logical_type = duckdb_create_logical_type(DUCKDB_TYPE_FLOAT);
        break;
    case READSTAT_TYPE_DOUBLE:
        if (format != NULL)
        {
            if (
                // SAS
                !strcmp(format, "WEEKDATE") || !strcmp(format, "MMDDYY") || !strcmp(format, "DDMMYY") || !strcmp(format, "YYMMDD") || !strcmp(format, "DATE") || !strcmp(format, "DATE9") || !strcmp(format, "YYMMDD10") || !strcmp(format, "DDMMYYB") || !strcmp(format, "DDMMYYB10") || !strcmp(format, "DDMMYYC") || !strcmp(format, "DDMMYYC10") || !strcmp(format, "DDMMYYD") || !strcmp(format, "DDMMYYD10") || !strcmp(format, "DDMMYYN6") || !strcmp(format, "DDMMYYN8") || !strcmp(format, "DDMMYYP") || !strcmp(format, "DDMMYYP10") || !strcmp(format, "DDMMYYS") || !strcmp(format, "DDMMYYS10") || !strcmp(format, "MMDDYYB") || !strcmp(format, "MMDDYYB10") || !strcmp(format, "MMDDYYC") || !strcmp(format, "MMDDYYC10") || !strcmp(format, "MMDDYYD") || !strcmp(format, "MMDDYYD10") || !strcmp(format, "MMDDYYN6") || !strcmp(format, "MMDDYYN8") || !strcmp(format, "MMDDYYP") || !strcmp(format, "MMDDYYP10") || !strcmp(format, "MMDDYYS") || !strcmp(format, "MMDDYYS10") || !strcmp(format, "WEEKDATX") || !strcmp(format, "DTDATE") || !strcmp(format, "IS8601DA") || !strcmp(format, "E8601DA") || !strcmp(format, "B8601DA") || !strcmp(format, "YYMMDDB") || !strcmp(format, "YYMMDDD") || !strcmp(format, "YYMMDDN") || !strcmp(format, "YYMMDDP") || !strcmp(format, "YYMMDDS")
                // SPSS without duplicates from SAS
                || !strcmp(format, "DATE8") || !strcmp(format, "DATE11") || !strcmp(format, "DATE12") || !strcmp(format, "ADATE") || !strcmp(format, "ADATE8") || !strcmp(format, "ADATE10") || !strcmp(format, "EDATE") || !strcmp(format, "EDATE8") || !strcmp(format, "EDATE10") || !strcmp(format, "JDATE") || !strcmp(format, "JDATE5") || !strcmp(format, "JDATE7") || !strcmp(format, "SDATE") || !strcmp(format, "SDATE8") || !strcmp(format, "SDATE10")
                // Stata
                || !strcmp(format, "%td") || !strcmp(format, "%d") || !strcmp(format, "%tdD_m_Y") || !strcmp(format, "%tdCCYY-NN-DD"))
            {
                logical_type = duckdb_create_logical_type(DUCKDB_TYPE_DATE);
            }
            else if (
                // SAS
                !strcmp(format, "DATETIME") || !strcmp(format, "DATETIME18") || !strcmp(format, "DATETIME19") || !strcmp(format, "DATETIME20") || !strcmp(format, "DATETIME21") || !strcmp(format, "DATETIME22") || !strcmp(format, "E8601DT") || !strcmp(format, "DATEAMPM") || !strcmp(format, "MDYAMPM") || !strcmp(format, "IS8601DT") || !strcmp(format, "B8601DT") || !strcmp(format, "B8601DN")
                // SPSS without duplicates from SAS
                || !strcmp(format, "DATETIME8") || !strcmp(format, "DATETIME17") || !strcmp(format, "DATETIME23.2") || !strcmp(format, "YMDHMS16") || !strcmp(format, "YMDHMS19") || !strcmp(format, "YMDHMS19.2") || !strcmp(format, "YMDHMS20")
                // Stata
                || !strcmp(format, "%tC") || !strcmp(format, "%tc"))
            {
                logical_type = duckdb_create_logical_type(DUCKDB_TYPE_TIMESTAMP);
            }
            else if (
                // SAS
                !strcmp(format, "TIME") || !strcmp(format, "HHMM") || !strcmp(format, "TIME20.3") || !strcmp(format, "TIME20") || !strcmp(format, "TIME5") || !strcmp(format, "TOD") || !strcmp(format, "TIMEAMPM") || !strcmp(format, "IS8601TM") || !strcmp(format, "E8601TM") || !strcmp(format, "B8601TM")
                // SPSS without duplicates from SAS
                || !strcmp(format, "DTIME") || !strcmp(format, "TIME8") || !strcmp(format, "TIME5") || !strcmp(format, "TIME11.2")
                // Stata
                || !strcmp(format, "%tcHH:MM:SS") || !strcmp(format, "%tcHH:MM"))
            {
                logical_type = duckdb_create_logical_type(DUCKDB_TYPE_TIME);
            }
            else
            {
                logical_type = duckdb_create_logical_type(DUCKDB_TYPE_DOUBLE);
            }
        }
        else
        {
            logical_type = duckdb_create_logical_type(DUCKDB_TYPE_DOUBLE);
        }
        break;
    }

    duckdb_bind_add_result_column(context->bind_info, name, logical_type);
    duckdb_destroy_logical_type(&logical_type);

    return READSTAT_HANDLER_OK;
}

void duckdb_read_stat_bind_handle_error(const char *error_message, void *ctx)
{
    duckdb_read_stat_bind_data *context = (duckdb_read_stat_bind_data *)ctx;

    duckdb_free(context->error_message);
    context->error_message = NULL;

    if (error_message == NULL)
    {
        return;
    }

    size_t length = strlen(error_message) + 1;
    context->error_message = (char *)duckdb_malloc(length);
    if (context->error_message != NULL)
    {
        memcpy(context->error_message, error_message, length);
    }
}

static void duckdb_read_stat_bind_data_free(void *ctx)
{
    duckdb_read_stat_bind_data *data = (duckdb_read_stat_bind_data *)ctx;

    if (!data)
    {
        return;
    }

    duckdb_free(data->error_message);
    duckdb_free((void *)data->path);
    duckdb_free((void *)data->format);
    duckdb_free((void *)data->encoding);

    /* duckdb_destroy_file_handle() also closes the file if it is still open. */
    if (data->file_handle)
    {
        duckdb_destroy_file_handle(&data->file_handle);
    }

    duckdb_free(data);
}


void duckdb_read_stat_bind(duckdb_bind_info info)
{
    /* Every resource is declared and nulled up front so the single cleanup
     * block below can release whatever has been acquired so far. */
    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = NULL;
    duckdb_value path_value = NULL;
    duckdb_value format_value = NULL;
    duckdb_value encoding_value = NULL;
    duckdb_client_context client_context = NULL;
    duckdb_file_system file_system = NULL;
    duckdb_file_open_options file_open_options = NULL;
    duckdb_file_handle file_handle = NULL; /* not owned by `data` yet */
    duckdb_read_stat_io_context *io_context = NULL;
    duckdb_read_stat_bind_data *data = NULL;

    parser = readstat_parser_init();
    if (parser == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: failed to create parser");
        goto cleanup;
    }

    /* The duckdb_value wrappers are owned by us; the strings they produce are
     * separate copies owned by the bind data. */
    path_value = duckdb_bind_get_parameter(info, 0);
    format_value = duckdb_bind_get_named_parameter(info, "format");
    encoding_value = duckdb_bind_get_named_parameter(info, "encoding");

    data = duckdb_malloc(sizeof(*data));
    if (data == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: out of memory");
        goto cleanup;
    }
    /* Zero everything so every pointer is safe to free and file_handle stays
     * NULL until we actually own a handle. */
    memset(data, 0, sizeof(*data));
    data->bind_info = info;

    data->path = duckdb_get_varchar(path_value);
    if (data->path == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: could not read path");
        goto cleanup;
    }

    duckdb_table_function_get_client_context(info, &client_context);
    file_system = duckdb_client_context_get_file_system(client_context);
    file_open_options = duckdb_create_file_open_options();
    duckdb_file_open_options_set_flag(file_open_options, DUCKDB_FILE_FLAG_READ, true);

    io_context = duckdb_malloc(sizeof(*io_context));
    if (io_context == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: out of memory");
        goto cleanup;
    }

    if (duckdb_file_system_open(file_system, data->path, file_open_options, &file_handle) != DuckDBSuccess ||
        file_handle == NULL)
    {
        file_handle = NULL;
        duckdb_bind_set_error(info, "read_stat: could not open file");
        goto cleanup;
    }

    /* From here on the bind data is the single owner of the handle;
     * io_context only borrows it. */
    data->file_handle = file_handle;
    file_handle = NULL;
    io_context->file_handle = data->file_handle;

    readstat_set_metadata_handler(parser, &duckdb_read_stat_bind_handle_metadata);
    readstat_set_variable_handler(parser, &duckdb_read_stat_bind_handle_variable);
    readstat_set_error_handler(parser, &duckdb_read_stat_bind_handle_error);
    readstat_set_io_ctx(parser, io_context);
    readstat_set_open_handler(parser, &duckdb_read_stat_handle_open);
    readstat_set_close_handler(parser, &duckdb_read_stat_handle_close);
    readstat_set_read_handler(parser, &duckdb_read_stat_handle_read);
    readstat_set_seek_handler(parser, &duckdb_read_stat_handle_seek);
    readstat_set_row_limit(parser, 0);

    if (encoding_value != NULL)
    {
        data->encoding = duckdb_get_varchar(encoding_value);
        if (data->encoding == NULL)
        {
            duckdb_bind_set_error(info, "read_stat: could not read encoding");
            goto cleanup;
        }
        readstat_set_file_character_encoding(parser, data->encoding);
    }

    if (format_value != NULL)
    {
        data->format = duckdb_get_varchar(format_value);
        if (data->format == NULL)
        {
            duckdb_bind_set_error(info, "read_stat: could not read format");
            goto cleanup;
        }
    }

    /* `error` starts at READSTAT_OK, so an unknown format is reported instead
     * of reading an uninitialized variable. */
    if (data->format != NULL)
    {
        if (!strcmp(data->format, "sas7bdat"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
            error = readstat_parse_sas7bdat(parser, data->path, data);
        }
        else if (!strcmp(data->format, "xpt"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
            error = readstat_parse_xport(parser, data->path, data);
        }
        else if (!strcmp(data->format, "sav") || !strcmp(data->format, "zsav"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
            error = readstat_parse_sav(parser, data->path, data);
        }
        else if (!strcmp(data->format, "por"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
            error = readstat_parse_por(parser, data->path, data);
        }
        else if (!strcmp(data->format, "dta"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_STATA;
            error = readstat_parse_dta(parser, data->path, data);
        }
        else
        {
            error = READSTAT_ERROR_PARSE; /* unknown format */
        }
    }
    else if (duckdb_read_stat_ends_with(data->path, ".sas7bdat"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
        error = readstat_parse_sas7bdat(parser, data->path, data);
    }
    else if (duckdb_read_stat_ends_with(data->path, ".xpt"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
        error = readstat_parse_xport(parser, data->path, data);
    }
    else if (duckdb_read_stat_ends_with(data->path, ".sav") || duckdb_read_stat_ends_with(data->path, ".zsav"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
        error = readstat_parse_sav(parser, data->path, data);
    }
    else if (duckdb_read_stat_ends_with(data->path, ".por"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
        error = readstat_parse_por(parser, data->path, data);
    }
    else if (duckdb_read_stat_ends_with(data->path, ".dta"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_STATA;
        error = readstat_parse_dta(parser, data->path, data);
    }
    else
    {
        error = READSTAT_ERROR_PARSE; /* unknown file extension */
    }

    if (error != READSTAT_OK)
    {
        duckdb_bind_set_error(
            info,
            data->error_message != NULL ? data->error_message : "read_stat: failed to read statistics file");
        goto cleanup;
    }

    duckdb_bind_set_cardinality(info, data->cardinality, true);

    /* DuckDB owns the bind data from here on and calls the recursive destructor
     * once the bind data is no longer needed. */
    duckdb_bind_set_bind_data(info, data, duckdb_read_stat_bind_data_free);
    data = NULL;

cleanup:
    /* Frees data plus path/format/encoding/error_message/file handle.
     * NULL-safe, so this also releases partially built data on error. */
    duckdb_read_stat_bind_data_free(data);

    /* Handle opened but never handed over to `data` (defensive). */
    if (file_handle != NULL)
    {
        duckdb_destroy_file_handle(&file_handle);
    }

    duckdb_free(io_context);
    if (file_open_options != NULL)
    {
        duckdb_destroy_file_open_options(&file_open_options);
    }
    if (file_system != NULL)
    {
        duckdb_destroy_file_system(&file_system);
    }
    if (client_context != NULL)
    {
        duckdb_destroy_client_context(&client_context);
    }
    if (path_value != NULL)
    {
        duckdb_destroy_value(&path_value);
    }
    if (format_value != NULL)
    {
        duckdb_destroy_value(&format_value);
    }
    if (encoding_value != NULL)
    {
        duckdb_destroy_value(&encoding_value);
    }
    readstat_parser_free(parser);
}



void duckdb_read_stat_init(duckdb_init_info info)
{
    duckdb_read_stat_init_data *data = duckdb_malloc(sizeof(duckdb_read_stat_init_data));
    data->offset = 0;
    data->actual_rows_read = 0;
    duckdb_init_set_init_data(info, data, duckdb_free);
}

const int days_between_unix_epoch_and_sas_epoch = -3653;
const int days_between_unix_epoch_and_spss_epoch = -141428;
const int days_between_unix_epoch_and_stata_epoch = -3653;

duckdb_date duckdb_read_stat_convert_date(int days, int seconds, int days_between_epochs)
{
    duckdb_date date;
    date.days = days_between_epochs + days + (seconds / (3600 * 24));
    return date;
}

double duckdb_read_stat_modulo(double dividend, double divisor)
{
    double remainder = fmod(dividend, divisor);
    remainder += ((remainder != 0) & ((remainder < 0) ^ (divisor < 0))) * divisor;
    return remainder;
}

duckdb_date duckdb_read_stat_to_date(double timestamp, duckdb_read_stat_file_format file_format)
{
    int days = 0;
    int seconds = 0;

    if (file_format == DUCKDB_READ_STAT_FILE_FORMAT_SPSS)
    {
        days = (int)(floor(timestamp / 86400.0));
        seconds = (int)(duckdb_read_stat_modulo(timestamp, 86400.0));
    }
    else
    {
        days = (int)timestamp;
    }

    switch (file_format)
    {
    case DUCKDB_READ_STAT_FILE_FORMAT_SAS:
        return duckdb_read_stat_convert_date(days, seconds, days_between_unix_epoch_and_sas_epoch);
    case DUCKDB_READ_STAT_FILE_FORMAT_SPSS:
        return duckdb_read_stat_convert_date(days, seconds, days_between_unix_epoch_and_spss_epoch);
    case DUCKDB_READ_STAT_FILE_FORMAT_STATA:
        return duckdb_read_stat_convert_date(days, seconds, days_between_unix_epoch_and_stata_epoch);
    }
}

duckdb_timestamp duckdb_read_stat_convert_timestamp(int days, int seconds, int days_between_epochs)
{
    duckdb_timestamp timestamp;
    timestamp.micros = ((((int64_t)days_between_epochs + (int64_t)days) * 24L * 3600L) + (int64_t)seconds) * 1000000L;
    return timestamp;
}

duckdb_timestamp duckdb_read_stat_to_timestamp(double timestamp, duckdb_read_stat_file_format file_format)
{
    int days = 0;
    double milliseconds = 0;
    int seconds = 0;

    if (file_format == DUCKDB_READ_STAT_FILE_FORMAT_STATA)
    {
        days = (int)(floor(timestamp / 86400000.0));
        milliseconds = duckdb_read_stat_modulo(timestamp, 86400000.0);
        seconds = (int)(milliseconds / 1000.0);
    }
    else
    {
        days = (int)(floor(timestamp / 86400.0));
        seconds = (int)duckdb_read_stat_modulo(timestamp, 86400.0);
    }

    switch (file_format)
    {
    case DUCKDB_READ_STAT_FILE_FORMAT_SAS:
        return duckdb_read_stat_convert_timestamp(days, seconds, days_between_unix_epoch_and_sas_epoch);
    case DUCKDB_READ_STAT_FILE_FORMAT_SPSS:
        return duckdb_read_stat_convert_timestamp(days, seconds, days_between_unix_epoch_and_spss_epoch);
    case DUCKDB_READ_STAT_FILE_FORMAT_STATA:
        return duckdb_read_stat_convert_timestamp(days, seconds, days_between_unix_epoch_and_stata_epoch);
    }
}

const int seconds_in_hour = 60 * 60;
const int seconds_in_minute = 60;

duckdb_time duckdb_read_stat_convert_time(int days, int seconds)
{
    int hours, minutes, remaining_seconds;
    int total = (24 * 3600 * days) + seconds;

    hours = (total / seconds_in_hour);
    remaining_seconds = total - (hours * seconds_in_hour);
    minutes = remaining_seconds / seconds_in_minute;
    remaining_seconds = remaining_seconds - (minutes * seconds_in_minute);

    duckdb_time_struct time_struct;

    time_struct.hour = hours;
    time_struct.min = minutes;
    time_struct.sec = remaining_seconds;
    time_struct.micros = 0;

    return duckdb_to_time(time_struct);
}

duckdb_time duckdb_read_stat_to_time(double tstamp, duckdb_read_stat_file_format file_format)
{
    int days = 0;
    int seconds = 0;
    double msecs = 0;
    int usecs = 0;

    if (file_format == DUCKDB_READ_STAT_FILE_FORMAT_STATA)
    {
        days = (int)(floor(tstamp / 86400000.0));
        msecs = duckdb_read_stat_modulo(tstamp, 86400000.0);
        seconds = (int)(msecs / 1000.0);
        usecs = (int)(duckdb_read_stat_modulo(msecs, 1000.0) * 1000.0);
    }
    else
    {
        days = (int)(floor(tstamp / 86400.0));
        seconds = (int)(duckdb_read_stat_modulo(tstamp, 86400.0));
    }

    return duckdb_read_stat_convert_time(days, seconds);
}

int duckdb_read_stat_handle_value(int obs_index, readstat_variable_t *variable, readstat_value_t value, void *ctx)
{
    duckdb_read_stat_context *context = (duckdb_read_stat_context *)ctx;
    duckdb_read_stat_bind_data *bind_data = duckdb_function_get_bind_data(context->function_info);
    duckdb_read_stat_init_data *init_data = duckdb_function_get_init_data(context->function_info);
    idx_t var_index = readstat_variable_get_index(variable);
    readstat_type_t type = readstat_value_type(value);
    const char *format = readstat_variable_get_format(variable);
    duckdb_vector vector = duckdb_data_chunk_get_vector(context->data_chunk, var_index);

    if (!readstat_value_is_system_missing(value) && !readstat_value_is_tagged_missing(value) && !readstat_value_is_defined_missing(value, variable))
    {
        switch (type)
        {
        case READSTAT_TYPE_STRING:
        case READSTAT_TYPE_STRING_REF:
            duckdb_vector_assign_string_element(vector, obs_index, readstat_string_value(value));
            break;

        case READSTAT_TYPE_INT8:
            ((int8_t *)duckdb_vector_get_data(vector))[obs_index] = readstat_int8_value(value);
            break;

        case READSTAT_TYPE_INT16:
            ((int16_t *)duckdb_vector_get_data(vector))[obs_index] = readstat_int16_value(value);
            break;

        case READSTAT_TYPE_INT32:
            ((int32_t *)duckdb_vector_get_data(vector))[obs_index] = readstat_int32_value(value);
            break;

        case READSTAT_TYPE_FLOAT:
            ((float *)duckdb_vector_get_data(vector))[obs_index] = readstat_float_value(value);
            break;

        case READSTAT_TYPE_DOUBLE:
        {
            double double_value = readstat_double_value(value);

            if (format != NULL)
            {
                if (
                    // SAS
                    !strcmp(format, "WEEKDATE") || !strcmp(format, "MMDDYY") || !strcmp(format, "DDMMYY") || !strcmp(format, "YYMMDD") || !strcmp(format, "DATE") || !strcmp(format, "DATE9") || !strcmp(format, "YYMMDD10") || !strcmp(format, "DDMMYYB") || !strcmp(format, "DDMMYYB10") || !strcmp(format, "DDMMYYC") || !strcmp(format, "DDMMYYC10") || !strcmp(format, "DDMMYYD") || !strcmp(format, "DDMMYYD10") || !strcmp(format, "DDMMYYN6") || !strcmp(format, "DDMMYYN8") || !strcmp(format, "DDMMYYP") || !strcmp(format, "DDMMYYP10") || !strcmp(format, "DDMMYYS") || !strcmp(format, "DDMMYYS10") || !strcmp(format, "MMDDYYB") || !strcmp(format, "MMDDYYB10") || !strcmp(format, "MMDDYYC") || !strcmp(format, "MMDDYYC10") || !strcmp(format, "MMDDYYD") || !strcmp(format, "MMDDYYD10") || !strcmp(format, "MMDDYYN6") || !strcmp(format, "MMDDYYN8") || !strcmp(format, "MMDDYYP") || !strcmp(format, "MMDDYYP10") || !strcmp(format, "MMDDYYS") || !strcmp(format, "MMDDYYS10") || !strcmp(format, "WEEKDATX") || !strcmp(format, "DTDATE") || !strcmp(format, "IS8601DA") || !strcmp(format, "E8601DA") || !strcmp(format, "B8601DA") || !strcmp(format, "YYMMDDB") || !strcmp(format, "YYMMDDD") || !strcmp(format, "YYMMDDN") || !strcmp(format, "YYMMDDP") || !strcmp(format, "YYMMDDS")
                    // SPSS without duplicates from SAS
                    || !strcmp(format, "DATE8") || !strcmp(format, "DATE11") || !strcmp(format, "DATE12") || !strcmp(format, "ADATE") || !strcmp(format, "ADATE8") || !strcmp(format, "ADATE10") || !strcmp(format, "EDATE") || !strcmp(format, "EDATE8") || !strcmp(format, "EDATE10") || !strcmp(format, "JDATE") || !strcmp(format, "JDATE5") || !strcmp(format, "JDATE7") || !strcmp(format, "SDATE") || !strcmp(format, "SDATE8") || !strcmp(format, "SDATE10")
                    // Stata
                    || !strcmp(format, "%td") || !strcmp(format, "%d") || !strcmp(format, "%tdD_m_Y") || !strcmp(format, "%tdCCYY-NN-DD"))
                {
                    duckdb_date converted = duckdb_read_stat_to_date(double_value, bind_data->file_format);
                    duckdb_date *output_value_timestamp = (duckdb_date *)duckdb_vector_get_data(vector);
                    output_value_timestamp[obs_index] = converted;
                }
                else if (
                    // SAS
                    !strcmp(format, "DATETIME") || !strcmp(format, "DATETIME18") || !strcmp(format, "DATETIME19") || !strcmp(format, "DATETIME20") || !strcmp(format, "DATETIME21") || !strcmp(format, "DATETIME22") || !strcmp(format, "E8601DT") || !strcmp(format, "DATEAMPM") || !strcmp(format, "MDYAMPM") || !strcmp(format, "IS8601DT") || !strcmp(format, "B8601DT") || !strcmp(format, "B8601DN")
                    // SPSS without duplicates from SAS
                    || !strcmp(format, "DATETIME8") || !strcmp(format, "DATETIME17") || !strcmp(format, "DATETIME23.2") || !strcmp(format, "YMDHMS16") || !strcmp(format, "YMDHMS19") || !strcmp(format, "YMDHMS19.2") || !strcmp(format, "YMDHMS20")
                    // Stata
                    || !strcmp(format, "%tC") || !strcmp(format, "%tc"))
                {
                    duckdb_timestamp converted = duckdb_read_stat_to_timestamp(double_value, bind_data->file_format);
                    duckdb_timestamp *output_value_timestamp = (duckdb_timestamp *)duckdb_vector_get_data(vector);
                    output_value_timestamp[obs_index] = converted;
                }
                else if (
                    // SAS
                    !strcmp(format, "TIME") || !strcmp(format, "HHMM") || !strcmp(format, "TIME20.3") || !strcmp(format, "TIME20") || !strcmp(format, "TIME5") || !strcmp(format, "TOD") || !strcmp(format, "TIMEAMPM") || !strcmp(format, "IS8601TM") || !strcmp(format, "E8601TM") || !strcmp(format, "B8601TM")
                    // SPSS without duplicates from SAS
                    || !strcmp(format, "DTIME") || !strcmp(format, "TIME8") || !strcmp(format, "TIME5") || !strcmp(format, "TIME11.2")
                    // Stata
                    || !strcmp(format, "%tcHH:MM:SS") || !strcmp(format, "%tcHH:MM"))
                {
                    duckdb_time converted = duckdb_read_stat_to_time(double_value, bind_data->file_format);
                    duckdb_time *output_value_timestamp = (duckdb_time *)duckdb_vector_get_data(vector);
                    output_value_timestamp[obs_index] = converted;
                }
                else
                {
                    double *output_value = (double *)duckdb_vector_get_data(vector);
                    output_value[obs_index] = double_value;
                }
            }
            else
            {
                double *output_value = (double *)duckdb_vector_get_data(vector);
                output_value[obs_index] = double_value;
            }
            break;
        }
        }
    }
    else
    {
        duckdb_vector_ensure_validity_writable(vector);
        uint64_t *validity = duckdb_vector_get_validity(vector);
        duckdb_validity_set_row_invalid(validity, obs_index);
    }

    init_data->actual_rows_read = obs_index + 1;

    return READSTAT_HANDLER_OK;
}

int duckdb_read_stat_handle_metadata(readstat_metadata_t *metadata, void *ctx)
{
    return READSTAT_HANDLER_OK;
}

int duckdb_read_stat_handle_variable(int index, readstat_variable_t *variable,
                                     const char *val_labels, void *ctx)
{
    return READSTAT_HANDLER_OK;
}

int duckdb_read_stat_handle_value_label(const char *val_labels, readstat_value_t value, const char *label, void *ctx)
{
    return READSTAT_HANDLER_OK;
}

void duckdb_read_stat_handle_error(const char *error_message, void *ctx)
{
    duckdb_read_stat_context *context = (duckdb_read_stat_context *)ctx;
    context->error_message = (char *)duckdb_malloc(strlen(error_message) + 1);
    strcpy(context->error_message, error_message);
}

int duckdb_read_stat_handle_open(const char *path, void *io_ctx)
{
    return READSTAT_HANDLER_OK;
}

int duckdb_read_stat_handle_close(void *io_ctx)
{
    return READSTAT_HANDLER_OK;
}

ssize_t duckdb_read_stat_handle_read(void *buf, size_t nbyte, void *io_ctx)
{
    duckdb_file_handle file_handle = ((duckdb_read_stat_io_context *)io_ctx)->file_handle;
    return duckdb_file_handle_read(file_handle, buf, nbyte);
}

readstat_off_t duckdb_read_stat_handle_seek(readstat_off_t offset, readstat_io_flags_t whence, void *io_ctx)
{
    duckdb_file_handle file_handle = ((duckdb_read_stat_io_context *)io_ctx)->file_handle;

    switch (whence)
    {
    case READSTAT_SEEK_CUR:
        return duckdb_file_handle_tell(file_handle) + duckdb_file_handle_seek(file_handle, duckdb_file_handle_tell(file_handle) + offset);
    case READSTAT_SEEK_SET:
        return duckdb_file_handle_seek(file_handle, offset);
    case READSTAT_SEEK_END:
        return duckdb_file_handle_seek(file_handle, duckdb_file_handle_size(file_handle) - offset);
    }
}

void duckdb_read_stat_function(duckdb_function_info info, duckdb_data_chunk output)
{
    duckdb_read_stat_init_data *init_data = (duckdb_read_stat_init_data *)duckdb_function_get_init_data(info);
    duckdb_read_stat_bind_data *bind_data = (duckdb_read_stat_bind_data *)duckdb_function_get_bind_data(info);

    if (init_data->offset >= bind_data->cardinality)
    {
        duckdb_data_chunk_set_size(output, 0);
        return;
    }

    readstat_error_t error = READSTAT_OK;
    readstat_parser_t *parser = readstat_parser_init();

    readstat_set_row_offset(parser, (long)init_data->offset);
    readstat_set_row_limit(parser, (long)duckdb_vector_size());
    readstat_set_metadata_handler(parser, &duckdb_read_stat_handle_metadata);
    readstat_set_variable_handler(parser, &duckdb_read_stat_handle_variable);
    readstat_set_value_handler(parser, &duckdb_read_stat_handle_value);
    readstat_set_error_handler(parser, &duckdb_read_stat_handle_error);
    duckdb_read_stat_io_context *io_context = duckdb_malloc(sizeof(duckdb_read_stat_io_context));
    duckdb_file_handle_seek(bind_data->file_handle, 0);
    io_context->file_handle = bind_data->file_handle;
    readstat_set_io_ctx(parser, (void *)io_context);
    readstat_set_open_handler(parser, &duckdb_read_stat_handle_open);
    readstat_set_close_handler(parser, &duckdb_read_stat_handle_close);
    readstat_set_read_handler(parser, &duckdb_read_stat_handle_read);
    readstat_set_seek_handler(parser, &duckdb_read_stat_handle_seek);

    duckdb_read_stat_context *context = duckdb_malloc(sizeof(duckdb_read_stat_context));
    context->function_info = info;
    context->data_chunk = output;
    context->error_message = NULL;

    if (bind_data->encoding != NULL)
    {
        readstat_set_file_character_encoding(parser, bind_data->encoding);
    }

    if (bind_data->format != NULL)
    {
        if (!strcasecmp(bind_data->format, "sas7bdat"))
        {
            error = readstat_parse_sas7bdat(parser, bind_data->path, context);
        }
        else if (!strcasecmp(bind_data->format, "xpt"))
        {
            error = readstat_parse_xport(parser, bind_data->path, context);
        }
        else if (!strcasecmp(bind_data->format, "sav") || !strcasecmp(bind_data->format, "zsav"))
        {
            error = readstat_parse_sav(parser, bind_data->path, context);
        }
        else if (!strcasecmp(bind_data->format, "por"))
        {
            error = readstat_parse_por(parser, bind_data->path, context);
        }
        else if (!strcasecmp(bind_data->format, "dta"))
        {
            error = readstat_parse_dta(parser, bind_data->path, context);
        }
    }
    else if (duckdb_read_stat_ends_with(bind_data->path, ".sas7bdat"))
    {
        error = readstat_parse_sas7bdat(parser, bind_data->path, context);
    }
    else if (duckdb_read_stat_ends_with(bind_data->path, ".xpt"))
    {
        error = readstat_parse_xport(parser, bind_data->path, context);
    }
    else if (duckdb_read_stat_ends_with(bind_data->path, ".sav") || duckdb_read_stat_ends_with(bind_data->path, ".zsav"))
    {
        error = readstat_parse_sav(parser, bind_data->path, context);
    }
    else if (duckdb_read_stat_ends_with(bind_data->path, ".por"))
    {
        error = readstat_parse_por(parser, bind_data->path, context);
    }
    else if (duckdb_read_stat_ends_with(bind_data->path, ".dta"))
    {
        error = readstat_parse_dta(parser, bind_data->path, context);
    }

    if (error != READSTAT_OK)
    {
        if (context->error_message != NULL)
        {
            duckdb_function_set_error(info, context->error_message);
            duckdb_free(context->error_message);
        }
        duckdb_file_handle_close(io_context->file_handle);
        duckdb_destroy_file_handle(&(io_context->file_handle));
        /* The bind data owned this handle and its destructor will run later;
         * clear the stale pointer so it is not destroyed a second time. */
        bind_data->file_handle = NULL;
        duckdb_free(io_context);
        duckdb_free(context);
        readstat_parser_free(parser);
        return;
    }

    readstat_parser_free(parser);

    init_data->offset += init_data->actual_rows_read;
    duckdb_data_chunk_set_size(output, init_data->actual_rows_read);
    init_data->actual_rows_read = 0;
    duckdb_free(io_context); // total bytes lost: 486 -> 446. 8 bytes per call, ~5 chunks, checks out?
    duckdb_free(context);
}

void duckdb_read_stat_register_read_stat_function(duckdb_connection connection)
{
    duckdb_table_function function = duckdb_create_table_function();
    duckdb_table_function_set_name(function, "read_stat");

    duckdb_logical_type varchar_type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
    duckdb_table_function_add_parameter(function, varchar_type);
    duckdb_table_function_add_named_parameter(function, "format", varchar_type);
    duckdb_table_function_add_named_parameter(function, "encoding", varchar_type);
    duckdb_destroy_logical_type(&varchar_type);

    duckdb_table_function_set_bind(function, &duckdb_read_stat_bind);
    duckdb_table_function_set_init(function, &duckdb_read_stat_init);
    duckdb_table_function_set_function(function, &duckdb_read_stat_function);

    duckdb_state result = duckdb_register_table_function(connection, function);

    duckdb_destroy_table_function(&function);
}

void duckdb_read_stat_replacement_scan(duckdb_replacement_scan_info info, const char *table_name, void *data)
{
    if (duckdb_read_stat_ends_with(table_name, ".sas7bdat") || duckdb_read_stat_ends_with(table_name, ".xpt") || duckdb_read_stat_ends_with(table_name, ".sav") || duckdb_read_stat_ends_with(table_name, ".zsav") || duckdb_read_stat_ends_with(table_name, ".por") || duckdb_read_stat_ends_with(table_name, ".dta"))
    {
        duckdb_replacement_scan_set_function_name(info, "read_stat");
        duckdb_replacement_scan_add_parameter(info, duckdb_create_varchar(table_name));
    }
}

DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info info, struct duckdb_extension_access *access)
{
    duckdb_read_stat_register_read_stat_function(connection);
    duckdb_add_replacement_scan(*(access->get_database(info)), &duckdb_read_stat_replacement_scan, NULL, NULL);
    return true;
}
