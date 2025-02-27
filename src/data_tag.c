/** @file
    Custom data tags for data struct.

    Copyright (C) 2021 Christian Zuckschwerdt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "data_tag.h"
#include "mongoose.h"
#include "jsmn.h"
#include "data.h"
#include "list.h"
#include "optparse.h"
#include "c_util.h" // for MIN()
#include "fileformat.h"
#include "fatal.h"

typedef struct gpsd_client {
    struct mg_connect_opts connect_opts;
    struct mg_connection *conn;
    int32_t prev_status;
    uint8_t address[253 + 6 + 1]; // dns max + port
    uint8_t const *init_str;
    uint8_t const *filter_str;
    uint8_t msg[1024]; // GPSd TPV should about 600 bytes
} gpsd_client_t;

// GPSd JSON mode
uint8_t const watch_json[] = "?WATCH={\"enable\":true,\"json\":true}\n";
uint8_t const filter_json[] = "{\"class\":\"TPV\",";
// GPSd NMEA mode
uint8_t const watch_nmea[] = "?WATCH={\"enable\":true,\"nmea\":true}\n";
uint8_t const filter_nmea[] = "$GPGGA,";

static void gpsd_client_line(gpsd_client_t *ctx, uint8_t *line)
{
    if (!ctx->filter_str || strncmp(line, ctx->filter_str, strlen(ctx->filter_str)) == 0) {
        snprintf(ctx->msg, sizeof(ctx->msg), "%s", line);
    }
}

static struct mg_connection *gpsd_client_connect(gpsd_client_t *ctx, struct mg_mgr *mgr);

static void gpsd_client_event(struct mg_connection *nc, int32_t ev, void *ev_data)
{
    // note that while shutting down the ctx is NULL
    gpsd_client_t *ctx = (gpsd_client_t *)nc->user_data;

    //if (ev != MG_EV_POLL)
    //    fprintf(stderr, "GPSd user handler got event %d\n", ev);

    switch (ev) {
    case MG_EV_CONNECT: {
        int32_t connect_status = *(int32_t *)ev_data;
        if (connect_status == 0) {
            // Success
            fprintf(stderr, "GPSd Connected...\n");
            if (ctx->init_str && *ctx->init_str) {
                mg_send(nc, ctx->init_str,(int32_t) strlen(ctx->init_str));
            }
        }
        else {
            // Error, print only once
            if (ctx && ctx->prev_status != connect_status)
                fprintf(stderr, "GPSd connect error: %s\n", strerror(connect_status));
        }
        if (ctx)
            ctx->prev_status = connect_status;
        break;
    }
    case MG_EV_RECV: {
        // Require a newline
        struct mbuf *io = &nc->recv_mbuf;
        // note: we could scan only the last *(int32_t *)ev_data bytes...
        uint8_t *eol = memchr(io->buf, '\n', io->len);
        if (eol) {
            size_t len = eol - io->buf + 1;
            // strip [\r]\n
            io->buf[len - 1] = '\0';
            if (len >= 2 && io->buf[len - 2] == '\r') {
                io->buf[len - 2] = '\0';
            }
            gpsd_client_line(ctx, io->buf);
            mbuf_remove(io, len); // Discard line from recv buffer
        }
        break;
    }
    case MG_EV_CLOSE:
        if (!ctx)
            break; // shutting down
        if (ctx->prev_status == 0)
            fprintf(stderr, "GPSd Connection failed...\n");
        // reconnect
        gpsd_client_connect(ctx, nc->mgr);
        break;
    }
}

static struct mg_connection *gpsd_client_connect(gpsd_client_t *ctx, struct mg_mgr *mgr)
{
    uint8_t const *error_string       = NULL;
    ctx->connect_opts.error_string = &error_string;
    //ctx->conn                      = mg_connect_opt(mgr, ctx->address, gpsd_client_event, ctx->connect_opts);
    ctx->connect_opts.error_string = NULL;
    if (!ctx->conn) {
        fprintf(stderr, "GPSd connect (%s) failed%s%s\n", ctx->address,
                error_string ? ": " : "", error_string ? error_string : "");
    }
    return ctx->conn;
}

//static gpsd_client_t *gpsd_client_init(uint8_t const *host, uint8_t const *port, uint8_t const *init_str, uint8_t const *filter_str, struct mg_mgr *mgr)
//{
//    gpsd_client_t *ctx;
//    ctx = calloc(1, sizeof(gpsd_client_t));
//    if (!ctx) {
//        WARN_CALLOC("gpsd_client_init()");
//        return NULL;
//    }
//
//    // if the host is an IPv6 address it needs quoting
//    if (strchr(host, ':'))
//        snprintf(ctx->address, sizeof(ctx->address), "[%s]:%s", host, port);
//    else
//        snprintf(ctx->address, sizeof(ctx->address), "%s:%s", host, port);
//
//    ctx->init_str = init_str;
//    ctx->filter_str = filter_str;
//    ctx->connect_opts.user_data = ctx;
//
//    if (!gpsd_client_connect(ctx, mgr)) {
//        exit(1);
//    }
//
//    return ctx;
//}

static void gpsd_client_free(gpsd_client_t *ctx)
{
    if (ctx && ctx->conn) {
        ctx->conn->user_data = NULL;
        ctx->conn->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
    free(ctx);
}

data_tag_t *data_tag_create(uint8_t *param, struct mg_mgr *mgr)
{
    data_tag_t *tag;
    tag = calloc(1, sizeof(data_tag_t));
    if (!tag) {
        WARN_CALLOC("data_tag_create()");
        return NULL;
    }

    uint8_t *p = param;
    asepcb(&p, '=', ','); // look for '=' but stop at ','
    if (p) {
        tag->key = param;
        tag->val = p;
    } else {
        tag->val = param;
    }

    //int32_t gpsd_mode = strncmp(tag->val, "gpsd", 4) == 0;
    //if (gpsd_mode || strncmp(tag->val, "tcp:", 4) == 0) {
    //    p          = arg_param(tag->val); // strip scheme
    //    uint8_t const *host = gpsd_mode ? "localhost" : NULL;
    //    uint8_t const *port = gpsd_mode ? "2947" : NULL;
    //    uint8_t *opts = hostport_param(p, &host, &port);
    //    list_t includes = {0};

    //    // default to GPSd JSON
    //    uint8_t const *mode = gpsd_mode ? "GPSd JSON" : "TCP custom";
    //    uint8_t const *init_str = gpsd_mode ? watch_json : NULL;
    //    uint8_t const *filter_str = gpsd_mode ? filter_json : NULL;
    //    // parse format options
    //    uint8_t *key, *val;
    //    while (getkwargs(&opts, &key, &val)) {
    //        key = remove_ws(key);
    //        val = trim_ws(val);
    //        if (!key || !*key)
    //            continue;
    //        else if (!strcasecmp(key, "nmea")) {
    //            mode       = "GPSd NMEA";
    //            init_str   = watch_nmea;
    //            filter_str = filter_nmea;
    //        }
    //        else if (!strcasecmp(key, "init")) {
    //            init_str = val;
    //        }
    //        else if (!strcasecmp(key, "filter")) {
    //            filter_str = val;
    //        }
    //        else if (val) {
    //            fprintf(stderr, "Invalid key \"%s\" option.\n", key);
    //            exit(1);
    //        }
    //        else {
    //            list_push(&includes, key);
    //        }
    //    }

    //    tag->includes = (uint8_t const **)includes.elems;
    //    if (!tag->key && !tag->includes)
    //        tag->key = gpsd_mode ? "gps" : "tag";

    //    if (!host || !port) {
    //        fprintf(stderr, "Host or port for tag client missing!\n");
    //        exit(1);
    //    }

    //    //fprintf(stderr, "Getting %s data from %s port %s\n", mode, host, port);

    //    //tag->gpsd_client = gpsd_client_init(host, port, init_str, filter_str, mgr);
    //}
    //else {
    //    if (!tag->key)
    //        tag->key = "tag";
    //}

    return tag; // NOTE: returns NULL on alloc failure.
}

void data_tag_free(data_tag_t *tag)
{
    free((void *)tag->includes);
    gpsd_client_free(tag->gpsd_client);

    free(tag);
}

static uint8_t const *find_list_strncmp(uint8_t const **list, uint8_t const *key, size_t len)
{
    for (; list && *list; ++list) {
        uint8_t const *elem = *list;
        if (elem && strncmp(elem, key, len) == 0) {
            return elem;
        }
    }
    return NULL;
}

#define MAX_JSON_TOKENS 128

static data_t *append_filtered_json(data_t *data, uint8_t const *json, uint8_t const **includes)
{
    jsmn_parser parser = {0};
    jsmn_init(&parser);
    jsmntok_t tok[MAX_JSON_TOKENS] = {{0}}; // make the compiler happy, should be {0}

    int32_t toks = jsmn_parse(&parser, json, strlen(json), tok, MAX_JSON_TOKENS);
    if (toks < 1 || tok[0].type != JSMN_OBJECT) {
        fprintf(stderr, "invalid json (%d): %s\n", toks, json);
        return data; // invalid json
    }

    // check all tokens
    for (int32_t i = 1; i < toks - 1; ++i) {
        jsmntok_t *k = tok + i;
        jsmntok_t *v = tok + i + 1;
        i += k->size + v->size;
        //fprintf(stderr, "TOK (%d %d) %.*s : (%d %d) %.*s\n",
        //        k->type, k->size, k->end - k->start, json + k->start,
        //        v->type, v->size, v->end - v->start, json + v->start);

        // check all includes
        uint8_t const *key = find_list_strncmp(includes, json + k->start, k->end - k->start);
        if (key) {
            // append json tag
            uint8_t buf[1024];
            int32_t len = MIN(v->end - v->start, (int32_t)sizeof(buf) - 1);
            memcpy(buf, json + v->start, len);
            buf[len] = '\0';
			data = data_str(data, key, "", NULL, buf);
        }
    }
    return data;
}

data_t *data_tag_apply(data_tag_t *tag, data_t *data, uint8_t const *filename)
{
    uint8_t const *val = tag->val;
    if (tag->gpsd_client) {
        val = tag->gpsd_client->msg;
        if (tag->includes) {
            if (tag->key) {
                // wrap tag includes
                data_t *obj = append_filtered_json(NULL, val, tag->includes);
                // append tag wrapper
				data = data_dat(data, tag->key, "", NULL, obj);
            }
            else {
                // append tag includes
                data = append_filtered_json(data, val, tag->includes);
            }
        }
        else {
            // append tag string
			data = data_str(data, tag->key, "", NULL, val);
        }
        return data;
    }
    else if (filename && !strcmp("PATH", tag->val)) {
        val = filename;
    }
    else if (filename && !strcmp("FILE", tag->val)) {
        val = file_basename(filename);
    }

    // prepend simple tags
    data = data_prepend(data,
            tag->key, "", DATA_STRING, val,
            NULL);

    return data;
}
