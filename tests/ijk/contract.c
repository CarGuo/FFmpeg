/* Regression contract for the ijkplayer additions to FFmpeg 5.1. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libavformat/avformat.h"
#include "libavformat/url.h"
#include "libavutil/application.h"
#include "libavutil/dict.h"
#include "libavutil/error.h"
#include "libavutil/mem.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    abort(); } } while (0)

#define PROTOCOL_DECL(x) int ijkav_register_##x##_protocol(URLProtocol *, int)
PROTOCOL_DECL(ijkio);
PROTOCOL_DECL(ijkhttphook);
PROTOCOL_DECL(ijktcphook);
PROTOCOL_DECL(ijklongurl);
PROTOCOL_DECL(ijkmediadatasource);
PROTOCOL_DECL(ijksegment);
PROTOCOL_DECL(async);
int ijkav_register_ijklivehook_demuxer(AVInputFormat *, int);
int ijkav_register_ijklas_demuxer(AVInputFormat *, int);
extern const URLProtocol ff_async_protocol;

static int opens, callback_count, last_event, segment_count;
static int segment_indices[8];
static AVAppHttpEvent http_event;
static AVAppAsyncStatistic async_stat;
static AVAppIOTraffic traffic;

static int event_callback(AVApplicationContext *h, int type, void *obj, size_t size)
{
    CHECK(h->opaque == &callback_count);
    ++callback_count;
    last_event = type;
    if (type == AVAPP_EVENT_DID_HTTP_OPEN) {
        CHECK(size == sizeof(http_event));
        http_event = *(AVAppHttpEvent *)obj;
    } else if (type == AVAPP_EVENT_ASYNC_STATISTIC) {
        CHECK(size == sizeof(async_stat));
        async_stat = *(AVAppAsyncStatistic *)obj;
    } else if (type == AVAPP_EVENT_IO_TRAFFIC) {
        CHECK(size == sizeof(traffic));
        traffic = *(AVAppIOTraffic *)obj;
    }
    return -987;
}

static void test_application(void)
{
    AVApplicationContext *ctx = NULL;
    AVAppAsyncStatistic stat = {sizeof(stat), 11, 22, 33};
    AVAppIOControl control = {0};
    CHECK(av_application_open(&ctx, &callback_count) == 0 && ctx);
    ctx->func_on_app_event = event_callback;
    av_application_did_http_open(ctx, ctx, "https://example.invalid/video", -9, 206, INT64_C(0x100000123));
    CHECK(last_event == AVAPP_EVENT_DID_HTTP_OPEN && callback_count == 1);
    CHECK(http_event.obj == ctx && http_event.error == -9 && http_event.http_code == 206);
    CHECK(http_event.filesize == INT64_C(0x100000123));
    CHECK(!strcmp(http_event.url, "https://example.invalid/video"));
    av_application_on_async_statistic(ctx, &stat);
    CHECK(async_stat.buf_backwards == 11 && async_stat.buf_forwards == 22 && async_stat.buf_capacity == 33);
    av_application_did_io_tcp_read(ctx, ctx, 19);
    CHECK(traffic.obj == ctx && traffic.bytes == 19);
    CHECK(av_application_on_io_control(ctx, AVAPP_CTRL_WILL_CONCAT_SEGMENT_OPEN, &control) == -987);
    CHECK(av_application_on_tcp_will_open(ctx) == -987);
    av_application_closep(&ctx);
    CHECK(!ctx);
    av_application_closep(&ctx);
    CHECK(av_application_on_io_control(NULL, 0, &control) == 0);
    puts("PASS application callback ABI and 64-bit filesize");
}

static void test_pointers(void)
{
    AVDictionary *dict = NULL;
    uintptr_t values[] = {0, 1, UINTPTR_MAX, UINTPTR_MAX / 2 + 1, (uintptr_t)&dict};
    size_t i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        char *text = av_dict_ptrtostr(values[i]);
        CHECK(text && !strncmp(text, "0x", 2));
        CHECK(av_dict_strtoptr(text) == values[i]);
        CHECK(av_dict_set_intptr(&dict, "ptr", values[i], AV_DICT_DONT_STRDUP_VAL) == 0);
        CHECK(av_dict_get_intptr(dict, "ptr") == values[i]);
        av_free(text);
    }
    CHECK(av_dict_get_intptr(dict, "missing") == 0);
    CHECK(av_dict_strtoptr(NULL) == 0);
    CHECK(av_dict_strtoptr("") == 0);
    CHECK(av_dict_strtoptr("1") == 0);
    CHECK(av_dict_strtoptr("0x1suffix") == 0);
    CHECK(av_dict_strtoptr("0x10000000000000000") == 0);
    CHECK(av_dict_strtoptr("0XAB") == 0xab);
    av_dict_free(&dict);
    puts("PASS pointer option roundtrips including high-bit addresses");
}

static int dummy_open(URLContext *h, const char *url, int flags, AVDictionary **options)
{
    (void)h; (void)url; (void)flags; (void)options;
    ++opens;
    return 0;
}
static int dummy_read(URLContext *h, unsigned char *buf, int size)
{
    (void)h; (void)buf; (void)size;
    return AVERROR_EOF;
}

static void test_protocol(const char *name, int (*register_protocol)(URLProtocol *, int))
{
    URLProtocol protocol = {.name = name, .url_open2 = dummy_open, .url_read = dummy_read};
    URLContext *ctx = NULL;
    char url[96];
    int before = opens;
    CHECK(register_protocol(&protocol, sizeof(protocol) - 1) < 0);
    CHECK(register_protocol(NULL, sizeof(protocol)) < 0);
    CHECK(register_protocol(&protocol, sizeof(protocol)) == 0);
    snprintf(url, sizeof(url), "%s:test", name);
    CHECK(ffurl_open_whitelist(&ctx, url, AVIO_FLAG_READ, NULL, NULL, name, NULL, NULL) == 0);
    CHECK(opens == before + 1);
    ffurl_closep(&ctx);
}

static int demux_header(AVFormatContext *ctx) { (void)ctx; return 0; }
static int demux_packet(AVFormatContext *ctx, AVPacket *pkt) { (void)ctx; (void)pkt; return AVERROR_EOF; }
static void test_demuxer(const char *name, int (*register_demuxer)(AVInputFormat *, int))
{
    AVInputFormat demuxer = {.name = name, .flags = AVFMT_NOFILE,
        .read_header = demux_header, .read_packet = demux_packet};
    const AVInputFormat *found;
    AVFormatContext *ctx = NULL;
    CHECK(register_demuxer(&demuxer, sizeof(demuxer) - 1) < 0);
    CHECK(register_demuxer(NULL, sizeof(demuxer)) < 0);
    CHECK(register_demuxer(&demuxer, sizeof(demuxer)) == 0);
    found = av_find_input_format(name);
    CHECK(found && found->read_header == demux_header);
    CHECK(avformat_open_input(&ctx, "unused", found, NULL) == 0);
    avformat_close_input(&ctx);
}

/* One millisecond of 8 kHz mono signed 16-bit PCM. */
static const unsigned char wav[] = {
    'R','I','F','F',52,0,0,0,'W','A','V','E',
    'f','m','t',' ',16,0,0,0,1,0,1,0,0x40,0x1f,0,0,
    0x80,0x3e,0,0,2,0,16,0,'d','a','t','a',16,0,0,0,
    0,0,1,0,2,0,3,0,4,0,5,0,6,0,7,0
};
typedef struct SegmentData { size_t pos; } SegmentData;
static int segment_open(URLContext *h, const char *url, int flags, AVDictionary **opts)
{
    AVDictionaryEntry *index = av_dict_get(*opts, "cur_file_no", NULL, 0);
    CHECK(index && segment_count < 8);
    segment_indices[segment_count++] = atoi(index->value);
    av_dict_set(opts, "cur_file_no", NULL, 0);
    h->is_streamed = 0;
    return 0;
}
static int segment_read(URLContext *h, unsigned char *buf, int size)
{
    SegmentData *data = h->priv_data;
    if (data->pos >= sizeof(wav)) return AVERROR_EOF;
    if ((size_t)size > sizeof(wav) - data->pos) size = (int)(sizeof(wav) - data->pos);
    memcpy(buf, wav + data->pos, size);
    data->pos += size;
    return size;
}
static int64_t segment_seek(URLContext *h, int64_t pos, int whence)
{
    SegmentData *data = h->priv_data;
    if (whence == AVSEEK_SIZE) return sizeof(wav);
    if (whence == SEEK_CUR) pos += data->pos;
    else if (whence == SEEK_END) pos += sizeof(wav);
    else if (whence != SEEK_SET) return AVERROR(EINVAL);
    if (pos < 0 || (uint64_t)pos > sizeof(wav)) return AVERROR(EINVAL);
    data->pos = pos;
    return pos;
}

static void test_concat(const char *path)
{
    URLProtocol protocol = {.name = "ijksegment", .url_open2 = segment_open,
        .url_read = segment_read, .url_seek = segment_seek, .priv_data_size = sizeof(SegmentData)};
    AVDictionary *options = NULL;
    AVFormatContext *ctx = NULL;
    AVPacket *packet = av_packet_alloc();
    FILE *file = fopen(path, "wb");
    int ret;
    CHECK(file && packet);
    fputs("ffconcat version 1.0\nfile 'ijksegment:0'\nfile 'ijksegment:1'\n", file);
    fclose(file);
    CHECK(ijkav_register_ijksegment_protocol(&protocol, sizeof(protocol)) == 0);
    av_dict_set(&options, "safe", "0", 0);
    av_dict_set(&options, "protocol_whitelist", "file,ijksegment", 0);
    CHECK(avformat_open_input(&ctx, path, av_find_input_format("concat"), &options) == 0);
    while ((ret = av_read_frame(ctx, packet)) >= 0) av_packet_unref(packet);
    CHECK(ret == AVERROR_EOF);
    CHECK(segment_count == 2 && segment_indices[0] == 0 && segment_indices[1] == 1);
    avformat_close_input(&ctx);
    av_packet_free(&packet);
    av_dict_free(&options);
    puts("PASS concat segment index propagation through registered URL protocol");
}

static void test_async(const char *path)
{
    URLProtocol protocol = {.name = "unused", .url_open2 = dummy_open};
    int (*original_open)(URLContext *, const char *, int, AVDictionary **) = ff_async_protocol.url_open2;
    AVIOContext *io = NULL;
    AVDictionary *options = NULL;
    unsigned char data[4];
    char url[4096];
    FILE *file = fopen(path, "wb");
    CHECK(file && fwrite(wav, 1, sizeof(wav), file) == sizeof(wav));
    fclose(file);
    CHECK(ijkav_register_async_protocol(&protocol, sizeof(protocol) - 1) < 0);
    CHECK(ijkav_register_async_protocol(&protocol, sizeof(protocol)) == 0);
    CHECK(ff_async_protocol.url_open2 == original_open && !strcmp(ff_async_protocol.name, "async"));
    snprintf(url, sizeof(url), "async:%s", path);
    av_dict_set(&options, "protocol_whitelist", "async,file", 0);
    CHECK(avio_open2(&io, url, AVIO_FLAG_READ, NULL, &options) == 0);
    CHECK(avio_read(io, data, sizeof(data)) == sizeof(data) && !memcmp(data, "RIFF", 4));
    avio_closep(&io);
    av_dict_free(&options);
    puts("PASS async no-op registration and built-in async file reading");
}

int main(int argc, char **argv)
{
    char path[4096];
    CHECK(argc == 2);
    test_application();
    test_pointers();
    test_protocol("ijkio", ijkav_register_ijkio_protocol);
    test_protocol("ijkhttphook", ijkav_register_ijkhttphook_protocol);
    test_protocol("ijktcphook", ijkav_register_ijktcphook_protocol);
    test_protocol("ijklongurl", ijkav_register_ijklongurl_protocol);
    test_protocol("ijkmediadatasource", ijkav_register_ijkmediadatasource_protocol);
    test_protocol("ijksegment", ijkav_register_ijksegment_protocol);
    puts("PASS six custom protocol registrations, open dispatch and ABI guards");
    test_demuxer("ijklivehook", ijkav_register_ijklivehook_demuxer);
    test_demuxer("ijklas", ijkav_register_ijklas_demuxer);
    puts("PASS two custom demuxer registrations and open dispatch");
    snprintf(path, sizeof(path), "%s/segments.ffconcat", argv[1]);
    test_concat(path);
    snprintf(path, sizeof(path), "%s/async.wav", argv[1]);
    test_async(path);
    puts("All ijk FFmpeg 5.1 contract tests passed.");
    return 0;
}
