/* Actual-source regression: include the selected upstream/patched MPJPEG demuxer.
 * Uses real FFmpeg 5 AVIO/format/packet APIs, not a replacement parser model. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef MPJPEG_SOURCE
#define MPJPEG_SOURCE "../../libavformat/mpjpegdec.c"
#endif
#include MPJPEG_SOURCE

#define JPEG "\xff\xd8\xff\xd9"
#define PART(b) "--" b "\r\nContent-Type: image/jpeg\r\nContent-Length: 4\r\n\r\n" JPEG "\r\n"
#define CLOSE(b) "--" b "--\r\n"
#define MAX_BOUNDARY "0123456789012345678901234567890123456789012345678901234567890123456789"

typedef struct Memory {
    const AVClass *av_class;
    char *mime_type;
    const unsigned char *data;
    int length, position, fault_after;
} Memory;

static const AVOption memory_options[] = {
    { "mime_type", "test MIME type", offsetof(Memory, mime_type), AV_OPT_TYPE_STRING,
      { .str = NULL }, 0, 0, AV_OPT_FLAG_DECODING_PARAM },
    { NULL }
};
static const AVClass memory_class = {
    .class_name = "mpjpeg regression MIME source", .item_name = av_default_item_name,
    .option = memory_options, .version = LIBAVUTIL_VERSION_INT
};
static void *memory_child(void *obj, void *previous)
{
    return previous ? NULL : ((AVIOContext *)obj)->opaque;
}
static const AVClass io_class = {
    .class_name = "mpjpeg regression AVIO", .item_name = av_default_item_name,
    .version = LIBAVUTIL_VERSION_INT, .child_next = memory_child
};
static int read_memory(void *opaque, uint8_t *dst, int requested)
{
    Memory *m = opaque;
    int amount;
    if (m->fault_after >= 0 && m->position >= m->fault_after)
        return AVERROR(EIO);
    if (m->position >= m->length)
        return AVERROR_EOF;
    amount = FFMIN(requested, m->length - m->position);
    if (m->fault_after >= 0)
        amount = FFMIN(amount, m->fault_after - m->position);
    memcpy(dst, m->data + m->position, amount);
    m->position += amount;
    return amount;
}
static int64_t seek_memory(void *opaque, int64_t offset, int whence)
{
    Memory *m = opaque;
    int64_t next;
    if (whence == AVSEEK_SIZE)
        return m->length;
    whence &= ~AVSEEK_FORCE;
    if (whence == SEEK_SET) next = offset;
    else if (whence == SEEK_CUR) next = m->position + offset;
    else if (whence == SEEK_END) next = m->length + offset;
    else return AVERROR(EINVAL);
    if (next < 0 || next > m->length) return AVERROR(EINVAL);
    m->position = next;
    return next;
}

typedef struct Test {
    const char *name, *boundary, *body;
    int strict, good_packets, terminal, fault_after, corrupt_packets, sticky, replay;
} Test;
static int run_case(const Test *t)
{
    Memory m = { .av_class = &memory_class, .data = (const unsigned char *)t->body,
                 .length = strlen(t->body), .fault_after = t->fault_after };
    AVFormatContext *fmt = avformat_alloc_context();
    AVIOContext *io = avio_alloc_context(av_malloc(256), 256, 0, &m,
                                        read_memory, NULL, seek_memory);
    AVDictionary *opts = NULL;
    AVPacket *pkt = av_packet_alloc();
    int ret, good = 0, corrupt = 0, terminal = 0, bad = 0;
    m.mime_type = av_asprintf("multipart/x-mixed-replace; boundary=%s", t->boundary);
    io->av_class = &io_class;
    fmt->pb = io;
    fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
    av_dict_set(&opts, "strict_mime_boundary", t->strict ? "1" : "0", 0);
    ret = avformat_open_input(&fmt, NULL, &ff_mpjpeg_demuxer, &opts);
    av_dict_free(&opts);
    if (ret < 0) terminal = ret;
    else for (int n = 0; n < 10; n++) {
        ret = fmt->iformat->read_packet(fmt, pkt);
        if (ret < 0) { terminal = ret; break; }
        if (pkt->size == 4 && !memcmp(pkt->data, JPEG, 4) &&
            !(pkt->flags & AV_PKT_FLAG_CORRUPT)) good++;
        else if (pkt->flags & AV_PKT_FLAG_CORRUPT) corrupt++;
        else bad++;
        av_packet_unref(pkt);
    }
    if (t->sticky && fmt && terminal == AVERROR_EOF) {
        int before = m.position;
        for (int i = 0; i < 3; i++)
            if (fmt->iformat->read_packet(fmt, pkt) != AVERROR_EOF) bad++;
        if (m.position != before) bad++;
    }
    if (t->replay && fmt && terminal == AVERROR_EOF) {
        int replay_good = 0, replay_end = 0;
        if (av_seek_frame(fmt, -1, 0, AVSEEK_FLAG_BYTE) < 0) bad++;
        else for (int n = 0; n < 10; n++) {
            int got = fmt->iformat->read_packet(fmt, pkt);
            if (got < 0) { replay_end = got; break; }
            if (pkt->size == 4 && !memcmp(pkt->data, JPEG, 4) &&
                !(pkt->flags & AV_PKT_FLAG_CORRUPT)) replay_good++;
            else bad++;
            av_packet_unref(pkt);
        }
        if (replay_good != t->good_packets || replay_end != AVERROR_EOF) bad++;
    }
    ret = good == t->good_packets && corrupt == t->corrupt_packets &&
          terminal == t->terminal && !bad;
    printf("%s %s strict=%d good=%d corrupt=%d terminal=%d expected=%d bad=%d\n",
           ret ? "PASS" : "FAIL", t->name, t->strict, good, corrupt,
           terminal, t->terminal, bad);
    av_packet_free(&pkt);
    avformat_close_input(&fmt);
    av_freep(&io->buffer);
    avio_context_free(&io);
    av_free(m.mime_type);
    return ret;
}
int main(void)
{
    const char *fault_body = PART("cam") PART("cam");
    int fault_position = strstr(fault_body, "\r\n\r\n") - fault_body + 4 + 2;
    const Test tests[] = {
        {"exact_final_permissive", "cam", PART("cam") PART("cam") CLOSE("cam"), 0,2,AVERROR_EOF,-1,0,1},
        {"exact_final_strict", "cam", PART("cam") PART("cam") CLOSE("cam"), 1,2,AVERROR_EOF,-1,0,1},
        {"final_epilogue_permissive", "cam", PART("cam") CLOSE("cam") "ignored epilogue\r\n", 0,1,AVERROR_EOF,-1,0,1},
        {"final_epilogue_strict", "cam", PART("cam") CLOSE("cam") "ignored epilogue\r\n", 1,1,AVERROR_EOF,-1,0,1},
        {"hyphen_token_permissive", "cam--", PART("cam--") PART("cam--") CLOSE("cam--"), 0,2,AVERROR_EOF,-1,0,1},
        {"hyphen_token_strict", "cam--", PART("cam--") PART("cam--") CLOSE("cam--"), 1,2,AVERROR_EOF,-1,0,1},
        {"transport_padding", "cam", PART("cam") "--cam-- \t\r\n", 1,1,AVERROR_EOF,-1,0,1},
        {"permissive_normal_boundary_change", "cam", PART("cam") PART("other") CLOSE("cam"), 0,2,AVERROR_EOF,-1,0,1},
        {"seek_after_eof_permissive", "cam", PART("cam") PART("cam") CLOSE("cam"), 0,2,AVERROR_EOF,-1,0,1,1},
        {"seek_after_eof_strict", "cam", PART("cam") PART("cam") CLOSE("cam"), 1,2,AVERROR_EOF,-1,0,1,1},
        {"max70_boundary_permissive", MAX_BOUNDARY, PART(MAX_BOUNDARY) CLOSE(MAX_BOUNDARY), 0,1,AVERROR_EOF,-1,0,1},
        {"max70_boundary_strict", MAX_BOUNDARY, PART(MAX_BOUNDARY) CLOSE(MAX_BOUNDARY), 1,1,AVERROR_EOF,-1,0,1},
        {"max70_initial_padding", MAX_BOUNDARY, "--" MAX_BOUNDARY " \t\r\nContent-Type: image/jpeg\r\nContent-Length: 4\r\n\r\n" JPEG "\r\n--" MAX_BOUNDARY "-- \t\r\n", 1,1,AVERROR_EOF,-1,0,1},
        {"strict_boundary_change_rejected", "cam", PART("cam") PART("other"), 1,1,AVERROR_INVALIDDATA,-1,0,0},
        {"missing_content_type", "cam", PART("cam") "--cam\r\nContent-Length: 4\r\n\r\n" JPEG, 0,1,AVERROR_INVALIDDATA,-1,0,0},
        {"non_jpeg_content_type", "cam", PART("cam") "--cam\r\nContent-Type: image/png\r\nContent-Length: 4\r\n\r\n" JPEG, 1,1,AVERROR_INVALIDDATA,-1,0,0},
        {"wrong_final_permissive", "cam", PART("cam") CLOSE("other"), 0,1,AVERROR_INVALIDDATA,-1,0,0},
        {"wrong_final_strict", "cam", PART("cam") CLOSE("other"), 1,1,AVERROR_INVALIDDATA,-1,0,0},
        {"truncated_header", "cam", PART("cam") "--cam\r\nContent-Type: image/jpeg", 1,1,AVERROR_INVALIDDATA,-1,0,0},
        {"between_parts_eof", "cam", PART("cam"), 0,1,AVERROR_EOF,-1,0,0},
        {"short_declared_payload", "cam", "--cam\r\nContent-Type: image/jpeg\r\nContent-Length: 8\r\n\r\n" JPEG, 1,0,AVERROR_EOF,-1,1,0},
        {"injected_eio_mid_payload", "cam", fault_body, 1,0,AVERROR(EIO),fault_position,1,0},
    };
    int passed = 0;
    av_log_set_level(AV_LOG_ERROR);
    for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) passed += run_case(&tests[i]);
    printf("SUMMARY passed=%d total=%zu\n", passed, sizeof(tests)/sizeof(tests[0]));
    return passed == sizeof(tests)/sizeof(tests[0]) ? 0 : 1;
}
