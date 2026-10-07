#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "libavformat/avformat.h"
#include "libavformat/rtsp.h"
#include "libavutil/mem.h"
#include "libavutil/error.h"
static AVFormatContext *context(const char *url) {
    AVFormatContext *s = avformat_alloc_context();
    RTSPState *rt = av_mallocz(sizeof(*rt));
    s->url = av_strdup(url); s->priv_data = rt;
    /* Synthetic state only: no credentials are sent over a socket. */
    strcpy(rt->auth, "fixture:local-only");
    strcpy(rt->session_id, "fixture-session-A");
    rt->auth_state.auth_type = HTTP_AUTH_BASIC;
    rt->get_parameter_supported = 1;
    return s;
}
static void close_context(AVFormatContext *s) {
    av_freep(&s->priv_data); avformat_free_context(s);
}
static int change(AVFormatContext *s, const char *url) {
    RTSPMessageHeader reply = {0};
    reply.status_code = 302;
    snprintf(reply.location, sizeof(reply.location), "%s", url);
    return ff_rtsp_apply_redirect(s, &reply);
}
int main(void) {
    AVFormatContext *s = context("rtsp://127.0.0.1:8554/one");
    RTSPState *rt = s->priv_data;
    assert(avformat_new_stream(s, NULL)); assert(avformat_new_stream(s, NULL));
    s->duration=60000000; s->start_time=10000000;
    av_dict_set(&s->metadata,"title","old SDP",0);
    av_dict_set(&s->metadata,"skip-calc-frame-rate","1",0);
    assert(change(s, "rtsp://127.0.0.1:8555/two") == 0);
    assert(!rt->auth[0] && rt->auth_state.auth_type == HTTP_AUTH_NONE);
    assert(!rt->session_id[0] && !rt->get_parameter_supported);
    assert(!s->nb_streams && !s->streams && rt->nb_redirects == 1);
    assert(s->duration==AV_NOPTS_VALUE && s->start_time==AV_NOPTS_VALUE);
    assert(!av_dict_get(s->metadata,"title",NULL,0));
    assert(av_dict_get(s->metadata,"skip-calc-frame-rate",NULL,0));
    close_context(s);
    s = context("rtsp://LOCALHOST:554/one"); rt=s->priv_data;
    assert(change(s, "rtsp://localhost/two") == 0);
    assert(rt->auth[0] && rt->auth_state.auth_type == HTTP_AUTH_BASIC);
    assert(!rt->session_id[0]);
    assert(change(s, "/relative") == AVERROR_INVALIDDATA);
    assert(!strcmp(s->url, "rtsp://localhost/two"));
    for (int i=1;i<8;i++) assert(change(s,"rtsp://localhost/two")==0);
    assert(change(s,"rtsp://localhost/two")==AVERROR(ELOOP));
    close_context(s);
    s=context("rtsps://localhost/one"); rt=s->priv_data;
    assert(change(s,"rtsp://localhost/two")==0);
    assert(!rt->auth[0]);close_context(s);
    puts("PASS redirect state: same-IP port change clears auth/session; same origin preserves auth; stream cleanup; absolute Location; eight-hop bound; scheme change");
    return 0;
}
