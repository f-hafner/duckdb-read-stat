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
    context->error_message = (char *)duckdb_malloc(strlen(error_message) + 1);
    strcpy(context->error_message, error_message);
}

static void duckdb_read_stat_bind_data_free(void *ptr)
{
    duckdb_read_stat_bind_data *data = (duckdb_read_stat_bind_data *)ptr;

    if (data == NULL)
        return;

    if (data->file_handle != NULL)
    {
        duckdb_file_handle_close(data->file_handle);
        duckdb_destroy_file_handle(&data->file_handle);
    }

    duckdb_free(data->error_message);
    duckdb_free((void *)data->path);
    duckdb_free((void *)data->format);
    duckdb_free((void *)data->encoding);
    duckdb_free(data);
}

static bool duckdb_read_stat_set_format(duckdb_read_stat_bind_data *data)
{
    const char *format = data->format;
    const char *path = data->path;

    if (format != NULL)
    {
        if (!strcmp(format, "sas7bdat"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
            data->readstat_format = READSTAT_FILE_FORMAT_SAS7BDAT;
        }
        else if (!strcmp(format, "xpt"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
            data->readstat_format = READSTAT_FILE_FORMAT_XPORT;
        }
        else if (!strcmp(format, "sav") || !strcmp(format, "zsav"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
            data->readstat_format = READSTAT_FILE_FORMAT_SAV;
        }
        else if (!strcmp(format, "por"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
            data->readstat_format = READSTAT_FILE_FORMAT_POR;
        }
        else if (!strcmp(format, "dta"))
        {
            data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_STATA;
            data->readstat_format = READSTAT_FILE_FORMAT_DTA;
        }
        else
        {
            return false;
        }
        return true;
    }

    if (duckdb_read_stat_ends_with(path, ".sas7bdat"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
        data->readstat_format = READSTAT_FILE_FORMAT_SAS7BDAT;
    }
    else if (duckdb_read_stat_ends_with(path, ".xpt"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SAS;
        data->readstat_format = READSTAT_FILE_FORMAT_XPORT;
    }
    else if (duckdb_read_stat_ends_with(path, ".sav") || duckdb_read_stat_ends_with(path, ".zsav"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
        data->readstat_format = READSTAT_FILE_FORMAT_SAV;
    }
    else if (duckdb_read_stat_ends_with(path, ".por"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_SPSS;
        data->readstat_format = READSTAT_FILE_FORMAT_POR;
    }
    else if (duckdb_read_stat_ends_with(path, ".dta"))
    {
        data->file_format = DUCKDB_READ_STAT_FILE_FORMAT_STATA;
        data->readstat_format = READSTAT_FILE_FORMAT_DTA;
    }
    else
    {
        return false;
    }

    return true;
}

static void duckdb_read_stat_init_data_free(void *ptr)
{
    duckdb_read_stat_init_data *data = (duckdb_read_stat_init_data *)ptr;

    if (data == NULL)
        return;

    if (data->reader != NULL)
        readstat_reader_close(data->reader);
    if (data->parser != NULL)
        readstat_parser_free(data->parser);

    duckdb_free(data->io_context);
    duckdb_free(data->context.error_message);
    duckdb_free(data);
}

void duckdb_read_stat_bind(duckdb_bind_info info)
{
    readstat_parser_t *parser = readstat_parser_init();
    duckdb_value path_value = duckdb_bind_get_parameter(info, 0);
    duckdb_value format_value = duckdb_bind_get_named_parameter(info, "format");
    duckdb_value encoding_value = duckdb_bind_get_named_parameter(info, "encoding");
    duckdb_read_stat_bind_data *data = duckdb_malloc(sizeof(duckdb_read_stat_bind_data));
    duckdb_read_stat_io_context io_context;
    duckdb_client_context client_context = NULL;
    duckdb_file_system file_system;
    duckdb_file_open_options file_open_options;
    duckdb_file_handle file_handle = NULL;
    readstat_reader_t *reader = NULL;
    readstat_error_t error = READSTAT_OK;

    if (data == NULL || parser == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: out of memory");
        if (parser != NULL)
            readstat_parser_free(parser);
        duckdb_free(data);
        return;
    }

    data->bind_info = info;
    data->path = duckdb_get_varchar(path_value);
    data->format = format_value != NULL ? duckdb_get_varchar(format_value) : NULL;
    data->encoding = encoding_value != NULL ? duckdb_get_varchar(encoding_value) : NULL;
    data->cardinality = 0;
    data->error_message = NULL;
    data->file_handle = NULL;

    duckdb_destroy_value(&path_value);
    if (format_value != NULL)
        duckdb_destroy_value(&format_value);
    if (encoding_value != NULL)
        duckdb_destroy_value(&encoding_value);

    if (data->path == NULL || !duckdb_read_stat_set_format(data))
    {
        duckdb_bind_set_error(info, "read_stat: could not determine file format");
        duckdb_read_stat_bind_data_free(data);
        readstat_parser_free(parser);
        return;
    }

    duckdb_table_function_get_client_context(info, &client_context);
    file_system = duckdb_client_context_get_file_system(client_context);
    duckdb_destroy_client_context(&client_context);
    file_open_options = duckdb_create_file_open_options();
    duckdb_file_open_options_set_flag(file_open_options, DUCKDB_FILE_FLAG_READ, true);
    if (duckdb_file_system_open(file_system, data->path, file_open_options, &file_handle) != DuckDBSuccess)
        file_handle = NULL;
    duckdb_destroy_file_open_options(&file_open_options);
    duckdb_destroy_file_system(&file_system);

    if (file_handle == NULL)
    {
        duckdb_bind_set_error(info, "read_stat: could not open file");
        duckdb_read_stat_bind_data_free(data);
        readstat_parser_free(parser);
        return;
    }

    io_context.file_handle = file_handle;

    readstat_set_metadata_handler(parser, &duckdb_read_stat_bind_handle_metadata);
    readstat_set_variable_handler(parser, &duckdb_read_stat_bind_handle_variable);
    readstat_set_error_handler(parser, &duckdb_read_stat_bind_handle_error);
    readstat_set_io_ctx(parser, &io_context);
    readstat_set_open_handler(parser, &duckdb_read_stat_handle_open);
    readstat_set_close_handler(parser, &duckdb_read_stat_handle_close);
    readstat_set_read_handler(parser, &duckdb_read_stat_handle_read);
    readstat_set_seek_handler(parser, &duckdb_read_stat_handle_seek);
    readstat_set_row_limit(parser, 0);

    if (data->encoding != NULL)
        readstat_set_file_character_encoding(parser, data->encoding);

    reader = readstat_reader_open(parser, data->readstat_format, data->path, data, &error);
    if (reader != NULL)
        readstat_reader_close(reader);
    readstat_parser_free(parser);

    if (error != READSTAT_OK)
    {
        duckdb_bind_set_error(info, data->error_message != NULL ? data->error_message : "read_stat: failed to read file metadata");
        duckdb_read_stat_bind_data_free(data);
        return;
    }

    data->file_handle = file_handle;
    duckdb_bind_set_cardinality(data->bind_info, data->cardinality, true);
    duckdb_bind_set_bind_data(info, data, duckdb_read_stat_bind_data_free);
}

void duckdb_read_stat_init(duckdb_init_info info)
{
    duckdb_read_stat_bind_data *bind_data = (duckdb_read_stat_bind_data *)duckdb_init_get_bind_data(info);
    duckdb_read_stat_init_data *data = duckdb_malloc(sizeof(duckdb_read_stat_init_data));
    readstat_parser_t *parser = readstat_parser_init();
    readstat_error_t error = READSTAT_OK;

    if (data == NULL || parser == NULL)
    {
        duckdb_init_set_error(info, "read_stat: out of memory");
        if (parser != NULL)
            readstat_parser_free(parser);
        duckdb_free(data);
        return;
    }

    memset(data, 0, sizeof(*data));
    data->io_context = duckdb_malloc(sizeof(duckdb_read_stat_io_context));
    if (data->io_context == NULL)
    {
        duckdb_init_set_error(info, "read_stat: out of memory");
        readstat_parser_free(parser);
        duckdb_free(data);
        return;
    }

    data->parser = parser;
    data->io_context->file_handle = bind_data->file_handle;
    data->context.bind_data = bind_data;
    data->context.data_chunk = NULL;
    data->context.slot = 0;
    data->context.last_obs_index = -1;
    data->context.error_message = NULL;

    duckdb_file_handle_seek(bind_data->file_handle, 0);

    readstat_set_metadata_handler(parser, &duckdb_read_stat_handle_metadata);
    readstat_set_variable_handler(parser, &duckdb_read_stat_handle_variable);
    readstat_set_value_handler(parser, &duckdb_read_stat_handle_value);
    readstat_set_error_handler(parser, &duckdb_read_stat_handle_error);
    readstat_set_io_ctx(parser, data->io_context);
    readstat_set_open_handler(parser, &duckdb_read_stat_handle_open);
    readstat_set_close_handler(parser, &duckdb_read_stat_handle_close);
    readstat_set_read_handler(parser, &duckdb_read_stat_handle_read);
    readstat_set_seek_handler(parser, &duckdb_read_stat_handle_seek);
    readstat_set_row_limit(parser, 0);

    if (bind_data->encoding != NULL)
        readstat_set_file_character_encoding(parser, bind_data->encoding);

    data->reader = readstat_reader_open(parser, bind_data->readstat_format, bind_data->path, &data->context, &error);
    if (data->reader == NULL)
    {
        duckdb_init_set_error(info, data->context.error_message != NULL ? data->context.error_message : "read_stat: failed to open file reader");
        readstat_parser_free(parser);
        duckdb_free(data->io_context);
        duckdb_free(data->context.error_message);
        duckdb_free(data);
        return;
    }

    duckdb_init_set_init_data(info, data, duckdb_read_stat_init_data_free);
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
    duckdb_read_stat_bind_data *bind_data = context->bind_data;
    idx_t var_index = readstat_variable_get_index(variable);
    readstat_type_t type = readstat_value_type(value);
    const char *format = readstat_variable_get_format(variable);
    idx_t row_index;
    duckdb_vector vector;

    if (obs_index != context->last_obs_index)
    {
        if (context->last_obs_index >= 0)
            context->slot++;
        context->last_obs_index = obs_index;
        context->row_count = context->slot + 1;
    }
    row_index = context->slot;
    vector = duckdb_data_chunk_get_vector(context->data_chunk, var_index);

    if (!readstat_value_is_system_missing(value) && !readstat_value_is_tagged_missing(value) && !readstat_value_is_defined_missing(value, variable))
    {
        switch (type)
        {
        case READSTAT_TYPE_STRING:
        case READSTAT_TYPE_STRING_REF:
            duckdb_vector_assign_string_element(vector, row_index, readstat_string_value(value));
            break;

        case READSTAT_TYPE_INT8:
            ((int8_t *)duckdb_vector_get_data(vector))[row_index] = readstat_int8_value(value);
            break;

        case READSTAT_TYPE_INT16:
            ((int16_t *)duckdb_vector_get_data(vector))[row_index] = readstat_int16_value(value);
            break;

        case READSTAT_TYPE_INT32:
            ((int32_t *)duckdb_vector_get_data(vector))[row_index] = readstat_int32_value(value);
            break;

        case READSTAT_TYPE_FLOAT:
            ((float *)duckdb_vector_get_data(vector))[row_index] = readstat_float_value(value);
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
                    output_value_timestamp[row_index] = converted;
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
                    output_value_timestamp[row_index] = converted;
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
                    output_value_timestamp[row_index] = converted;
                }
                else
                {
                    double *output_value = (double *)duckdb_vector_get_data(vector);
                    output_value[row_index] = double_value;
                }
            }
            else
            {
                double *output_value = (double *)duckdb_vector_get_data(vector);
                output_value[row_index] = double_value;
            }
            break;
        }
        }
    }
    else
    {
        duckdb_vector_ensure_validity_writable(vector);
        uint64_t *validity = duckdb_vector_get_validity(vector);
        duckdb_validity_set_row_invalid(validity, row_index);
    }

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

    duckdb_free(context->error_message);
    context->error_message = NULL;

    if (error_message == NULL)
        return;

    context->error_message = (char *)duckdb_malloc(strlen(error_message) + 1);
    if (context->error_message != NULL)
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
    int64_t bytes_read = duckdb_file_handle_read(file_handle, buf, (int64_t)nbyte);
    return bytes_read < 0 ? -1 : (ssize_t)bytes_read;
}

readstat_off_t duckdb_read_stat_handle_seek(readstat_off_t offset, readstat_io_flags_t whence, void *io_ctx)
{
    duckdb_file_handle file_handle = ((duckdb_read_stat_io_context *)io_ctx)->file_handle;
    readstat_off_t position;

    switch (whence)
    {
    case READSTAT_SEEK_CUR:
        position = duckdb_file_handle_tell(file_handle) + offset;
        break;
    case READSTAT_SEEK_SET:
        position = offset;
        break;
    case READSTAT_SEEK_END:
        position = duckdb_file_handle_size(file_handle) + offset;
        break;
    default:
        return -1;
    }

    if (position < 0)
        return -1;
    if (duckdb_file_handle_seek(file_handle, position) != DuckDBSuccess)
        return -1;
    return position;
}

void duckdb_read_stat_function(duckdb_function_info info, duckdb_data_chunk output)
{
    duckdb_read_stat_init_data *init_data = (duckdb_read_stat_init_data *)duckdb_function_get_init_data(info);
    duckdb_read_stat_context *context = &init_data->context;
    readstat_error_t error;

    if (context->error_message != NULL)
    {
        duckdb_function_set_error(info, context->error_message);
        duckdb_data_chunk_set_size(output, 0);
        return;
    }

    if (init_data->reader == NULL || readstat_reader_is_eof(init_data->reader))
    {
        duckdb_data_chunk_set_size(output, 0);
        return;
    }

    context->data_chunk = output;
    context->slot = 0;
    context->row_count = 0;
    context->last_obs_index = -1;

    error = readstat_reader_read_rows(init_data->reader, duckdb_vector_size());
    if (error != READSTAT_OK)
    {
        duckdb_function_set_error(info, context->error_message != NULL ? context->error_message : readstat_error_message(error));
        duckdb_data_chunk_set_size(output, 0);
        return;
    }

    duckdb_data_chunk_set_size(output, context->row_count);
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
