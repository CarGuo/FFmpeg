/*
 * Copyright (c) 2026
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/* Inspect retained HTTP options without starting a network connection. */
#include "libavformat/http.c"
#undef OFFSET
#include "libavformat/hls.c"
#include "libavformat/tls.h"

#include <stdio.h>

static int failures;
static int open_count;
static int expect_tls;
static int expect_empty_proxy;

typedef struct TLSHolder {
    const AVClass *class;
    TLSShared tls;
} TLSHolder;

static const AVOption test_tls_options[] = {
    TLS_COMMON_OPTIONS(TLSHolder, tls), { NULL }
};
static const AVClass test_tls_class = {
    .class_name = "TLS option test",
    .item_name = av_default_item_name,
    .option = test_tls_options,
    .version = LIBAVUTIL_VERSION_INT,
};

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #c); failures++; \
} } while (0)

static int value_is(const AVDictionary *dict, const char *key, const char *value)
{
    const AVDictionaryEntry *e = av_dict_get(dict, key, NULL, AV_DICT_MATCH_CASE);
    return value ? e && !strcmp(e->value, value) : !e;
}

static void check_effective_ca(const AVDictionary *options, const char *expected)
{
    TLSHolder holder = { .class = &test_tls_class };
    AVDictionary *tmp = NULL;

    av_opt_set_defaults(&holder);
    CHECK(av_dict_copy(&tmp, options, 0) == 0);
    CHECK(av_opt_set_dict(&holder, &tmp) == 0);
    CHECK(holder.tls.ca_file && !strcmp(holder.tls.ca_file, expected));
    av_dict_free(&tmp);
    av_opt_free(&holder);
}

static void check_replay(void)
{
    HTTPContext http = { 0 }, nested = { 0 };
    URLProtocol protocol = { .name = "https" };
    URLContext url = { .prot = &protocol, .priv_data = &http };
    AVDictionary *opts = NULL, *unchanged;
    int ret;

    av_dict_set(&http.chained_options, "tls_verify", "1", 0);
    av_dict_set(&http.chained_options, "ca_file", "first.pem", 0);
    av_dict_set(&http.chained_options, "cafile", "second.pem", 0);
    av_dict_set(&http.chained_options, "timeout", "10000", 0);
    av_dict_set(&http.chained_options, "protocol_whitelist", "https,tls,tcp", 0);
    av_dict_copy(&opts, http.chained_options, 0);
    CHECK(http_restore_options(&http, &opts) == 0);
    check_effective_ca(opts, "second.pem");
    av_dict_free(&opts);

    CHECK(ff_http_copy_tls_options(&url, &nested.chained_options) == 0);
    av_dict_copy(&opts, nested.chained_options, 0);
    CHECK(http_restore_options(&nested, &opts) == 0);
    check_effective_ca(opts, "second.pem");
    /* The saved settings still win over different pending settings. */
    av_dict_set(&opts, "tls_verify", "0", 0);
    av_dict_set(&opts, "offset", "123", 0);
    CHECK(http_restore_options(&nested, &opts) == 0);
    CHECK(value_is(opts, "tls_verify", "1"));
    CHECK(value_is(opts, "offset", "123"));
    check_effective_ca(opts, "second.pem");
    unchanged = opts;
    av_max_alloc(1);
    ret = http_restore_options(&nested, &opts);
    av_max_alloc(INT_MAX);
    CHECK(ret == AVERROR(ENOMEM) && opts == unchanged);
    CHECK(value_is(opts, "tls_verify", "1"));
    check_effective_ca(opts, "second.pem");
    av_dict_free(&opts);
    av_dict_free(&nested.chained_options);
    av_dict_free(&http.chained_options);
}

static void check_tls(const AVDictionary *dict, const char *verify)
{
    CHECK(value_is(dict, "tls_verify", verify));
    CHECK(value_is(dict, "ca_file", "custom-ca.pem"));
    CHECK(value_is(dict, "cafile", "alias-ca.pem"));
    CHECK(value_is(dict, "verifyhost", "peer.example"));
    CHECK(value_is(dict, "cert_file", "client-cert.pem"));
    CHECK(value_is(dict, "key_file", "client-key.pem"));
    CHECK(value_is(dict, "listen", NULL));
    CHECK(value_is(dict, "unrelated_demuxer_option", NULL));
}

/* All HLS resource paths terminate here, before any protocol or socket open. */
static int capture_open(AVFormatContext *s, AVIOContext **pb, const char *url,
                        int flags, AVDictionary **options)
{
    open_count++;
    CHECK(flags == AVIO_FLAG_READ);
    CHECK(s->protocol_whitelist && !strcmp(s->protocol_whitelist, "https,tls,tcp,crypto"));
    CHECK(s->protocol_blacklist && !strcmp(s->protocol_blacklist, "file"));
    if (expect_tls) {
        check_tls(*options, "1");
        check_effective_ca(*options, "alias-ca.pem");
    } else
        CHECK(value_is(*options, "tls_verify", NULL));
    if (expect_empty_proxy)
        CHECK(value_is(*options, "http_proxy", ""));
    CHECK(value_is(*options, "headers", "X-Test: inherited\r\n"));
    *pb = NULL;
    return AVERROR_EXIT;
}

#if CONFIG_HTTP_PROTOCOL
static void close_fake_io(AVFormatContext *s, AVIOContext *pb)
{
    av_freep(&pb->buffer);
    avio_context_free(&pb);
}

static void check_keepalive_fallback(AVFormatContext *parent, AVDictionary *options)
{
    HTTPContext http = { .class = &http_context_class };
    URLContext url = { .av_class = &ffurl_context_class,
                       .prot = &ff_http_protocol, .priv_data = &http,
                       .flags = AVIO_FLAG_READ, .is_streamed = 1 };
    AVIOContext *pb = NULL;
    const char *target = "https://media.example/fallback.ts";
    int ret;

    /* A cached redirect with a zero limit fails after HTTP options were
     * consumed, but before http_open_cnx can touch the network. */
    http.location = av_strdup(target);
    av_dict_set(&http.redirect_cache, target,
                "9223372036854775807;https://media.example/redirect.ts", 0);
    CHECK(ffio_fdopen(&pb, &url) == 0);
    if (!pb)
        goto cleanup;
    parent->io_close = close_fake_io;
    open_count = 0;
    ret = open_url(parent, &pb, target, &options, NULL, NULL);
    CHECK(ret == AVERROR_EXIT && open_count == 1 && !pb);
    parent->io_close = NULL;
cleanup:
    av_opt_free(&http);
    av_freep(&http.uri);
    av_dict_free(&http.redirect_cache);
}
#endif

static void check_hls_opens(AVDictionary *options)
{
    AVFormatContext parent = { 0 };
    HLSContext c = { 0 };
    struct playlist pls = { 0 };
    struct segment seg = { 0 }, init = { 0 };
    AVIOContext *in = NULL;
    int ret;

    parent.priv_data = &c;
    parent.io_open = capture_open;
    parent.protocol_whitelist = "https,tls,tcp,crypto";
    parent.protocol_blacklist = "file";
    c.ctx = &parent;
    c.avio_opts = options;
    c.allowed_extensions = "ALL";
    pls.parent = &parent;
    seg.url = "https://media.example/seg.ts";
    seg.key = "https://keys.example/key.bin";
    seg.size = -1;
    init.url = "https://media.example/init.mp4";
    init.size = -1;

    /* Child master/media playlists and reloads use this same entry point. */
    open_count = 0;
    ret = parse_playlist(&c, "https://media.example/child.m3u8", &pls, NULL);
    CHECK(ret == AVERROR_EXIT && open_count == 1);
    c.http_persistent = 1;
    ret = parse_playlist(&c, "https://media.example/live.m3u8", &pls, NULL);
    CHECK(ret == AVERROR_EXIT && open_count == 2);
#if CONFIG_HTTP_PROTOCOL
    check_keepalive_fallback(&parent, options);
#endif

    open_count = 0;
    ret = open_input(&c, &pls, &seg, &in);
    CHECK(ret == AVERROR_EXIT && open_count == 1);
    seg.key_type = KEY_AES_128;
    open_count = 0;
    ret = open_input(&c, &pls, &seg, &in);
    CHECK(ret == AVERROR_EXIT && open_count == 2); /* key, then crypto+HTTPS */
    seg.key_type = KEY_SAMPLE_AES;
    pls.key_url[0] = 0;
    open_count = 0;
    ret = open_input(&c, &pls, &seg, &in);
    CHECK(ret == AVERROR_EXIT && open_count == 2);
    seg.init_section = &init;
    open_count = 0;
    ret = update_init_section(&pls, &seg);
    CHECK(ret == AVERROR_EXIT && open_count == 1);
}

static void check_context(const URLProtocol *protocol, const AVClass *class)
{
    HTTPContext http = { 0 };
    URLContext url = { 0 };
    AVIOContext *pb = NULL;
    AVDictionary *out = NULL;
    const AVDictionaryEntry *e;
    int ret, ca_position, alias_position, position;

    http.class = class;
    http.headers = "X-Test: inherited\r\n";
    url.av_class = &ffurl_context_class;
    url.prot = protocol;
    url.priv_data = &http;
    url.flags = AVIO_FLAG_READ;
    url.is_streamed = 1;
    CHECK(ffio_fdopen(&pb, &url) == 0);
    if (!pb)
        return;

    /* No TLS policy or proxy override is invented when absent. */
    CHECK(ffio_copy_url_options(pb, &out) == 0);
    CHECK(value_is(out, "tls_verify", NULL));
    CHECK(value_is(out, "http_proxy", NULL));
    av_dict_free(&out);

    av_dict_set(&http.chained_options, "ca_file", "custom-ca.pem", 0);
    av_dict_set(&http.chained_options, "tls_verify", "1", 0);
    av_dict_set(&http.chained_options, "cafile", "alias-ca.pem", 0);
    av_dict_set(&http.chained_options, "verifyhost", "peer.example", 0);
    av_dict_set(&http.chained_options, "cert_file", "client-cert.pem", 0);
    av_dict_set(&http.chained_options, "key_file", "client-key.pem", 0);
    av_dict_set(&http.chained_options, "listen", "1", 0);
    av_dict_set(&http.chained_options, "unrelated_demuxer_option", "private", 0);
    http.http_proxy = "";
    /* A redirect can leave HTTP(S) with no active TLS child. */
    http.location = "http://redirect.example/final.m3u8";
    CHECK(http.hd == NULL);
    CHECK(ffio_copy_url_options(pb, &out) == 0);
    check_tls(out, "1");
    CHECK(value_is(out, "http_proxy", ""));
    CHECK(value_is(out, "headers", "X-Test: inherited\r\n"));
    expect_tls = expect_empty_proxy = 1;
    check_hls_opens(out);

    ca_position = alias_position = -1;
    position = 0;
    e = NULL;
    while ((e = av_dict_get(out, "", e, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(e->key, "ca_file")) ca_position = position;
        if (!strcmp(e->key, "cafile")) alias_position = position;
        position++;
    }
    CHECK(ca_position >= 0 && alias_position > ca_position);
    av_dict_free(&out);
    /* Exercise the reverse alias order as well as explicit verification off. */
    av_dict_free(&http.chained_options);
    av_dict_set(&http.chained_options, "cafile", "alias-ca.pem", 0);
    av_dict_set(&http.chained_options, "ca_file", "custom-ca.pem", 0);
    av_dict_set(&http.chained_options, "tls_verify", "0", 0);
    av_dict_set(&http.chained_options, "verifyhost", "peer.example", 0);
    av_dict_set(&http.chained_options, "cert_file", "client-cert.pem", 0);
    av_dict_set(&http.chained_options, "key_file", "client-key.pem", 0);
    http.http_proxy = "http://proxy.example:8080";
    CHECK(ffio_copy_url_options(pb, &out) == 0);
    check_tls(out, "0");
    CHECK(value_is(out, "http_proxy", "http://proxy.example:8080"));
    ca_position = alias_position = -1;
    position = 0;
    e = NULL;
    while ((e = av_dict_get(out, "", e, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(e->key, "ca_file")) ca_position = position;
        if (!strcmp(e->key, "cafile")) alias_position = position;
        position++;
    }
    CHECK(alias_position >= 0 && ca_position > alias_position);
    av_dict_free(&out);

    /* Failed option retrieval/copy must not fall back to unsafe defaults. */
    av_max_alloc(1);
    ret = ffio_copy_url_options(pb, &out);
    av_max_alloc(INT_MAX);
    CHECK(ret == AVERROR(ENOMEM));
    av_dict_free(&out);
    av_max_alloc(1);
    ret = ff_http_copy_tls_options(&url, &out);
    av_max_alloc(INT_MAX);
    CHECK(ret == AVERROR(ENOMEM));
    av_dict_free(&out);

    av_dict_free(&http.chained_options);
    av_freep(&pb->buffer);
    avio_context_free(&pb);
}

int main(void)
{
    AVDictionary *out = NULL;
    URLProtocol protocol = { .name = "file" };
    URLContext url = { .prot = &protocol };

    av_log_set_level(AV_LOG_QUIET);
    check_replay();
    CHECK(ff_http_copy_tls_options(NULL, &out) == 0 && !out);
    CHECK(ff_http_copy_tls_options(&url, &out) == 0 && !out);
#if CONFIG_HTTP_PROTOCOL
    check_context(&ff_http_protocol, &http_context_class);
#endif
#if CONFIG_HTTPS_PROTOCOL
    check_context(&ff_https_protocol, &https_context_class);
#endif
    if (failures)
        return 1;
    printf("HLS TLS option inheritance: PASS (no network I/O)\n");
    return 0;
}
