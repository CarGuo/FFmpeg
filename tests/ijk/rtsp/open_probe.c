/* Small demux-only RTSP regression probe; source-linked FFmpeg test, no player UI. */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavutil/time.h"

typedef struct { int64_t deadline; } Deadline;
static int interrupted(void *opaque) {
    return av_gettime_relative() >= ((Deadline *)opaque)->deadline;
}
int main(int argc, char **argv) {
    int i, requested = 3, timeout_ms = 1200;
    const char *v = getenv("PROBE_PACKETS");
    if (v) requested = atoi(v);
    v = getenv("PROBE_DEADLINE_MS");
    if (v) timeout_ms = atoi(v);
    av_log_set_level(AV_LOG_WARNING);
    for (i = 1; i < argc; ++i) {
        AVFormatContext *s = avformat_alloc_context();
        AVDictionary *opts = NULL;
        AVPacket *pkt = av_packet_alloc();
        int deadline_ms = i == 1 && getenv("PROBE_FIRST_DEADLINE_MS") ? atoi(getenv("PROBE_FIRST_DEADLINE_MS")) : timeout_ms;
        Deadline d = {av_gettime_relative() + deadline_ms * 1000LL};
        int64_t start = av_gettime_relative();
        int ret, read_ret = 0, packets = 0, first_byte = -1;
        char err[256] = {0};
        s->interrupt_callback.callback = interrupted;
        s->interrupt_callback.opaque = &d;
        av_dict_set(&opts, "rtsp_transport", getenv("PROBE_TRANSPORT") ? getenv("PROBE_TRANSPORT") : "tcp", 0);
        av_dict_set(&opts, "timeout", getenv("PROBE_SOCKET_TIMEOUT") ? getenv("PROBE_SOCKET_TIMEOUT") : "300000", 0);
        ret = avformat_open_input(&s, argv[i], NULL, &opts);
        if (ret >= 0) {
            while (packets < requested && (read_ret = av_read_frame(s, pkt)) >= 0) {
                if (!packets && pkt->size) first_byte = pkt->data[0];
                ++packets;
                av_packet_unref(pkt);
            }
        }
        av_strerror(ret < 0 ? ret : read_ret, err, sizeof(err));
        printf("{\"open\":%d,\"open_ret\":%d,\"read_ret\":%d,\"packets\":%d,\"first_byte\":%d,\"streams\":%u,\"duration\":%"PRId64",\"elapsed_ms\":%.3f,\"error\":\"%s\"}\n",
               i, ret, read_ret, packets, first_byte, s ? s->nb_streams : 0, s ? s->duration : AV_NOPTS_VALUE,
               (av_gettime_relative() - start) / 1000.0, err);
        fflush(stdout);
        av_packet_free(&pkt);
        av_dict_free(&opts);
        avformat_close_input(&s);
        if (getenv("PROBE_BETWEEN_OPENS_MS")) av_usleep(1000 * atoi(getenv("PROBE_BETWEEN_OPENS_MS")));
    }
    return 0;
}
