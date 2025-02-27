/** @file
    File outputs for rtl_433 events.

    Copyright (C) 2021 Christian Zuckschwerdt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/* Modified by Marc Prieur for plugin sdrsharp (marco40_github@sfr.fr)

History : V1.00 2021-04-01 - First release
         V1.5.0.1 2023-01 

 All text above must be included in any redistribution.
*/
#include "dll_rtl_433.h" //for fprintf
#include "output_file.h"

#include "data.h"
#include "term_ctl.h"
#include "r_util.h"
#include "logger.h"
#include "fatal.h"
#include "decoder_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "r_device.h"
/* JSON printer */

typedef struct {
    struct data_output output;
    FILE *file;
} data_output_json_t;

static void R_API_CALLCONV print_json_array(data_output_t *output, data_array_t *array, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_json_t *json = (data_output_json_t *)output;

    fprintf(json->file, "[");
    for (int32_t c = 0; c < array->num_values; ++c) {
        if (c)
            fprintf(json->file, ", ");
        print_array_value(output, array, format, c);
    }
    fprintf(json->file, "]");
}

static void R_API_CALLCONV print_json_data(data_output_t *output, data_t *data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_json_t *json = (data_output_json_t *)output;

    bool separator = false;
    fputc('{', json->file);
    while (data) {
        if (separator)
            fprintf(json->file, ", ");
        output->print_string(output, data->key, NULL, NULL);
        fprintf(json->file, " : ");
        print_value(output, data->type, data->value, data->format);
        separator = true;
        data      = data->next;
    }
    fputc('}', json->file);
}

static void R_API_CALLCONV print_json_string(data_output_t *output, const uint8_t *str, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_json_t *json = (data_output_json_t *)output;

    size_t str_len = strlen(str);
    if (str[0] == '{' && str[str_len - 1] == '}') {
        // Print embedded JSON object verbatim
        fprintf(json->file, "%s", str);
        return;
    }

    fprintf(json->file, "\"");
    for (; *str; ++str) {
        if (*str == '\r') {
            fprintf(json->file, "\\r");
        }
        else if (*str == '\n') {
            fprintf(json->file, "\\n");
        }
        else if (*str == '\t') {
            fprintf(json->file, "\\t");
        }
        else if (*str == '"' || *str == '\\') {
            fputc('\\', json->file);
            fputc(*str, json->file);
        }
        else {
            fputc(*str, json->file);
        }
    }
    fprintf(json->file, "\"");
}

static void R_API_CALLCONV print_json_double(data_output_t *output, double data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_json_t *json = (data_output_json_t *)output;

    fprintf(json->file, "%.3f", data);
}

static void R_API_CALLCONV print_json_int(data_output_t *output, int32_t data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_json_t *json = (data_output_json_t *)output;

    fprintf(json->file, "%d", data);
}

static void R_API_CALLCONV data_output_json_print(data_output_t *output, data_t *data, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_json_t *json = (data_output_json_t *)output;

    if (json && json->file) {
        json->output.print_data(output, data, NULL, NULL);
        fputc('\n', json->file);
        fflush(json->file);
    }
}

static void R_API_CALLCONV data_output_json_free(data_output_t *output)
{
    if (!output)
        return;

    free(output);
}

struct data_output *data_output_json_create(int32_t log_level, FILE *file)
{
    data_output_json_t *json = calloc(1, sizeof(data_output_json_t));
    if (!json) {
        WARN_CALLOC("data_output_json_create()");
        return NULL; // NOTE: returns NULL on alloc failure.
    }

    json->output.log_level    = log_level;
    json->output.print_data   = print_json_data;
    json->output.print_array  = print_json_array;
    json->output.print_string = print_json_string;
    json->output.print_double = print_json_double;
    json->output.print_int    = print_json_int;
    json->output.output_print = data_output_json_print;
    json->output.output_free  = data_output_json_free;
    json->file                = file;

    return (struct data_output *)json;
}

/* Pretty Key-Value printer */

static int32_t kv_color_for_key(uint8_t const *key)
{
    if (!key || !*key)
        return TERM_COLOR_RESET;
    if (!strcmp(key, "tag") || !strcmp(key, "time"))
        return TERM_COLOR_BLUE;
    if (!strcmp(key, "model") || !strcmp(key, "type") || !strcmp(key, "id"))
        return TERM_COLOR_RED;
    if (!strcmp(key, "mic"))
        return TERM_COLOR_CYAN;
    if (!strcmp(key, "mod") || !strcmp(key, "freq") || !strcmp(key, "freq1") || !strcmp(key, "freq2"))
        return TERM_COLOR_MAGENTA;
    if (!strcmp(key, "rssi") || !strcmp(key, "snr") || !strcmp(key, "noise"))
        return TERM_COLOR_YELLOW;
    return TERM_COLOR_GREEN;
}

static int32_t kv_break_before_key(uint8_t const *key)
{
    if (!key || !*key)
        return 0;
    if (!strcmp(key, "model") || !strcmp(key, "mod") || !strcmp(key, "rssi") || !strcmp(key, "codes"))
        return 1;
    return 0;
}

static int32_t kv_break_after_key(uint8_t const *key)
{
    if (!key || !*key)
        return 0;
    if (!strcmp(key, "id") || !strcmp(key, "mic"))
        return 1;
    return 0;
}

typedef struct {
    struct data_output output;
    FILE *file;
    void *term;
    int32_t color;
    int32_t ring_bell;
    int32_t term_width;
    int32_t data_recursion;
    int32_t column;
} data_output_kv_t;

#define KV_SEP "_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ "

	static void R_API_CALLCONV print_kv_data(data_output_t *output, data_t *data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_kv_t *kv = (data_output_kv_t *)output;
    int32_t color     = kv->color;
    int32_t ring_bell = kv->ring_bell;
    int32_t is_log    = 0;
    // top-level: update width and print separator
    if (!kv->data_recursion) {
        // collect well-known top level keys
        data_t *data_src = NULL;
        data_t *data_lvl = NULL;
        data_t *data_msg = NULL;
        for (data_t *d = data; d; d = d->next) {
            if (!strcmp(d->key, "src"))
                data_src = d;
            else if (!strcmp(d->key, "lvl"))
                data_lvl = d;
            else if (!strcmp(d->key, "msg"))
                data_msg = d;
        }
        is_log = data_src && data_lvl && data_msg;
    }
    uint16_t i    = 0;
    uint8_t cara[LENLINES] = {""};
    for (; data; data = data->next) {
        // skip logging keys
        if (is_log && (!strcmp(data->key, "time") || !strcmp(data->key, "src") || !strcmp(data->key, "lvl") || !strcmp(data->key, "msg") || !strcmp(data->key, "num_rows"))) {
            continue;
        }
        uint8_t *key = *data->pretty_key ? data->pretty_key : data->key;
		snprintf(cara, LENLINES, "%-10s: ", key);
		strcpy((char *)ptrDeviceToPlugin->Key_Device[i], cara);
		traitementValue(data->type, data->value, data->format, cara);
		strcpy((char *)ptrDeviceToPlugin->Value_Device[i], cara);
        i++;
		if (i == nbLine)
			//realloc to nbline+NBLINES
		{
			ptrDeviceToPlugin->Key_Device = (uint8_t **)realloc(ptrDeviceToPlugin->Key_Device, (nbLine+ NBLINES) * sizeof(intptr_t));
			for (int32_t i = nbLine; i < nbLine+ NBLINES; i++)
				ptrDeviceToPlugin->Key_Device[i] = (uint8_t*)calloc(LENLINES, sizeof(uint8_t));
			ptrDeviceToPlugin->Value_Device = (uint8_t **)realloc(ptrDeviceToPlugin->Value_Device, (nbLine+ NBLINES) * sizeof(intptr_t));
			for (int32_t i = nbLine; i < nbLine+ NBLINES; i++)
				ptrDeviceToPlugin->Value_Device[i] = (uint8_t*)calloc(LENLINES, sizeof(uint8_t));
			nbLine += NBLINES;
		}
    }
	{ 
		ptrDeviceToPlugin->nbInfosDevice = i;
		fctInfosToPlugin(ptrDeviceToPlugin);
	}
    --kv->data_recursion;
}
static void R_API_CALLCONV print_kv_array(data_output_t *output, data_array_t *array, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_kv_t *kv = (data_output_kv_t *)output;

    //fprintf(kv->file, "[ ");
    for (int32_t c = 0; c < array->num_values; ++c) {
        if (c)
            fprintf(kv->file, ", ");
        print_array_value(output, array, format, c);
    }
    //fprintf(kv->file, " ]");
}
static void R_API_CALLCONV print_kv_double(data_output_t *output, double data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_kv_t *kv = (data_output_kv_t *)output;

    kv->column += fprintf(kv->file, format ? format : "%.3f", data);
}
static void R_API_CALLCONV print_kv_int(data_output_t *output, int32_t data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_kv_t *kv = (data_output_kv_t *)output;

    kv->column += fprintf(kv->file, format ? format : "%d", data);
}
static void R_API_CALLCONV print_kv_string(data_output_t *output, const uint8_t *data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_kv_t *kv = (data_output_kv_t *)output;

    kv->column += fprintf(kv->file, format ? format : "%s", data);
}
static void R_API_CALLCONV data_output_kv_print(data_output_t *output, data_t *data, defDeviceToPlugin *ptrDeviceToPlugin)
{
	if (!ptrDeviceToPlugin)
		return;
    data_output_kv_t *kv = (data_output_kv_t *)output;

    if (kv && kv->file) {
        kv->output.print_data(output, data, NULL,ptrDeviceToPlugin );
        //fputc('\n', kv->file);--------->out to execute window
        fflush(kv->file);
    }
}

static void R_API_CALLCONV data_output_kv_free(data_output_t *output)
{
    data_output_kv_t *kv = (data_output_kv_t *)output;

    if (!output)
        return;

    if (kv->color)
        term_free(kv->term);

    free(output);
}
struct data_output *data_output_kv_create(int32_t log_level, FILE *file)
{
    data_output_kv_t *kv = calloc(1, sizeof(data_output_kv_t));
    if (!kv) {
        WARN_CALLOC("data_output_kv_create()");
        return NULL; // NOTE: returns NULL on alloc failure.
    }

    kv->output.log_level    = log_level;
    kv->output.print_data   = print_kv_data;
    kv->output.print_array  = print_kv_array;
    kv->output.print_string = print_kv_string;
    kv->output.print_double = print_kv_double;
    kv->output.print_int    = print_kv_int;
    kv->output.output_print = data_output_kv_print;
    kv->output.output_free  = data_output_kv_free;
    kv->file                = file;

 /*   kv->term  = term_init(file);
    kv->color = term_has_color(kv->term);*/

    kv->ring_bell = 0; // TODO: enable if requested...

    return (struct data_output *)kv;
}

/* CSV printer */

typedef struct {
    struct data_output output;
    FILE *file;
    const uint8_t **fields;
    const uint8_t *separator;
} data_output_csv_t;

static void R_API_CALLCONV print_csv_data(data_output_t *output, data_t *data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_csv_t *csv = (data_output_csv_t *)output;

    fputc('{', csv->file);
    for (bool separator = false; data; data = data->next) {
        if (separator)
            fprintf(csv->file, "; "); // NOTE: distinct from csv->separator
        output->print_string(output, data->key, NULL, NULL);
        fprintf(csv->file, ": ");
        print_value(output, data->type, data->value, data->format);
        separator = true;
    }
    fputc('}', csv->file);
}

static void R_API_CALLCONV print_csv_array(data_output_t *output, data_array_t *array, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_csv_t *csv = (data_output_csv_t *)output;

    for (int32_t c = 0; c < array->num_values; ++c) {
        if (c)
            fprintf(csv->file, ";");
        print_array_value(output, array, format, c);
    }
}

static void R_API_CALLCONV print_csv_string(data_output_t *output, const uint8_t *str, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_csv_t *csv = (data_output_csv_t *)output;

    while (str && *str) {
        if (strncmp(str, csv->separator, strlen(csv->separator)) == 0)
            fputc('\\', csv->file);
        fputc(*str, csv->file);
        ++str;
    }
}

static int32_t compare_strings(const void *a, const void *b)
{
    return strcmp(*(uint8_t **)a, *(uint8_t **)b);
}

static void R_API_CALLCONV data_output_csv_start(struct data_output *output, uint8_t const *const *fields, int32_t num_fields)
{
    data_output_csv_t *csv = (data_output_csv_t *)output;

    int32_t csv_fields = 0;
    int32_t i, j;
    const uint8_t **allowed = NULL;
    int32_t *use_count       = NULL;
    int32_t num_unique_fields;
    if (!csv)
        goto alloc_error;

    csv->separator = ",";

    allowed = calloc(num_fields, sizeof(const uint8_t *));
    if (!allowed) {
        WARN_CALLOC("data_output_csv_start()");
        goto alloc_error;
    }
    memcpy((void *)allowed, fields, sizeof(const uint8_t *) * num_fields);

    qsort((void *)allowed, num_fields, sizeof(uint8_t *), compare_strings);

    // overwrite duplicates
    i = 0;
    j = 0;
    while (j < num_fields) {
        while (j > 0 && j < num_fields &&
                strcmp(allowed[j - 1], allowed[j]) == 0)
            ++j;

        if (j < num_fields) {
            allowed[i] = allowed[j];
            ++i;
            ++j;
        }
    }
    num_unique_fields = i;

    csv->fields = calloc(num_unique_fields + 1, sizeof(const uint8_t *));
    if (!csv->fields) {
        WARN_CALLOC("data_output_csv_start()");
        goto alloc_error;
    }

    use_count = calloc(num_unique_fields + 1, sizeof(*use_count)); // '+ 1' so we never alloc size 0
    if (!use_count) {
        WARN_CALLOC("data_output_csv_start()");
        goto alloc_error;
    }

    for (i = 0; i < num_fields; ++i) {
        const uint8_t **field   = bsearch(&fields[i], allowed, num_unique_fields, sizeof(const uint8_t *),
                compare_strings);
        int32_t *field_use_count = use_count + (field - allowed);
        if (field && !*field_use_count) {
            csv->fields[csv_fields] = fields[i];
            ++csv_fields;
            ++*field_use_count;
        }
    }
    csv->fields[csv_fields] = NULL;
    free((void *)allowed);
    free(use_count);

    // Output the CSV header
    for (i = 0; csv->fields[i]; ++i) {
        fprintf(csv->file, "%s%s", i > 0 ? csv->separator : "", csv->fields[i]);
    }
    fprintf(csv->file, "\n");
    return;

alloc_error:
    free(use_count);
    free((void *)allowed);
    if (csv)
        free((void *)csv->fields);
    free(csv);
}

static void R_API_CALLCONV print_csv_double(data_output_t *output, double data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_csv_t *csv = (data_output_csv_t *)output;

    fprintf(csv->file, "%.3f", data);
}

static void R_API_CALLCONV print_csv_int(data_output_t *output, int32_t data, uint8_t const *format, defDeviceToPlugin *ptrDeviceToPlugin)
{
    UNUSED(format);
    data_output_csv_t *csv = (data_output_csv_t *)output;

    fprintf(csv->file, "%d", data);
}

static void R_API_CALLCONV data_output_csv_print(data_output_t *output, data_t *data, defDeviceToPlugin *ptrDeviceToPlugin)
{
    data_output_csv_t *csv = (data_output_csv_t *)output;

    const uint8_t **fields = csv->fields;

    int32_t regular = 0; // skip "states" output
    for (data_t *d = data; d; d = d->next) {
        if (!strcmp(d->key, "msg") || !strcmp(d->key, "codes") || !strcmp(d->key, "model")) {
            regular = 1;
            break;
        }
    }
    if (!regular)
        return;

    for (int32_t i = 0; fields[i]; ++i) {
        const uint8_t *key = fields[i];
        data_t *found   = NULL;
        if (i)
            fprintf(csv->file, "%s", csv->separator);
        for (data_t *iter = data; !found && iter; iter = iter->next)
            if (strcmp(iter->key, key) == 0)
                found = iter;

        if (found)
            print_value(output, found->type, found->value, found->format);
    }

    fputc('\n', csv->file);
    fflush(csv->file);
}

static void R_API_CALLCONV data_output_csv_free(data_output_t *output)
{
    data_output_csv_t *csv = (data_output_csv_t *)output;

    free((void *)csv->fields);
    free(csv);
}

struct data_output *data_output_csv_create(int32_t log_level, FILE *file)
{
    data_output_csv_t *csv = calloc(1, sizeof(data_output_csv_t));
    if (!csv) {
        WARN_CALLOC("data_output_csv_create()");
        return NULL; // NOTE: returns NULL on alloc failure.
    }

    csv->output.log_level    = log_level;
    csv->output.print_data   = print_csv_data;
    csv->output.print_array  = print_csv_array;
    csv->output.print_string = print_csv_string;
    csv->output.print_double = print_csv_double;
    csv->output.print_int    = print_csv_int;
    csv->output.output_start = data_output_csv_start;
    csv->output.output_print = data_output_csv_print;
    csv->output.output_free  = data_output_csv_free;
    csv->file                = file;

    return (struct data_output *)csv;
}
//static uint8_t * R_API_CALLCONV get_print_double(double data, uint8_t const *format)
//{
//    return fprintf("%.3f", data);
//}
//
//static uint8_t *R_API_CALLCONV get_print_int(int32_t data, uint8_t const *format)
//{
//    return fprintf("%d", data);
//}
//
//static uint8_t *R_API_CALLCONV get_print_string(const uint8_t *data, uint8_t const *format)
//{
//    return fprintf("%s", data);
//}
