/* New regression for the reconstructed MOV seek replay change. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavutil/md5.h"
#include "libavutil/pixdesc.h"
#include "libavutil/error.h"
#define CHECK(x) do { int rc=(x); if(rc<0){char b[128];av_strerror(rc,b,sizeof b);fprintf(stderr,"line %d: %s: %s\n",__LINE__,#x,b);exit(1);} }while(0)
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct Digest {int w,h;unsigned char md5[16];} Digest;
static Digest decode(AVBSFContext *b,AVCodecContext *c,AVPacket *p){
    AVPacket *o=av_packet_alloc();AVFrame *f=av_frame_alloc();struct AVMD5 *md=av_md5_alloc();Digest d;
    CHECK(av_bsf_send_packet(b,p));CHECK(av_bsf_receive_packet(b,o));CHECK(avcodec_send_packet(c,o));CHECK(avcodec_receive_frame(c,f));
    d.w=f->width;d.h=f->height;REQUIRE(f->format==AV_PIX_FMT_YUV420P);av_md5_init(md);
    for(int plane=0;plane<3;plane++){int w=plane?(f->width+1)/2:f->width;int h=plane?(f->height+1)/2:f->height;for(int y=0;y<h;y++)av_md5_update(md,f->data[plane]+y*f->linesize[plane],w);}
    av_md5_final(md,d.md5);av_free(md);av_packet_free(&o);av_frame_free(&f);return d;
}
int main(int argc,char **argv){
    REQUIRE(argc==3);AVFormatContext *s=NULL;AVDictionary *opts=NULL;AVBSFContext *b[2]={0};AVCodecContext *c[2]={0};AVPacket *p=av_packet_alloc();Digest expected[2][12];int seen[2]={0},checks=0;
    av_dict_set(&opts,"seek_streams_individually",argv[2],0);CHECK(avformat_open_input(&s,argv[1],NULL,&opts));av_dict_free(&opts);REQUIRE(s->nb_streams>=1&&s->nb_streams<=2);
    for(unsigned k=0;k<s->nb_streams;k++){CHECK(av_bsf_alloc(av_bsf_get_by_name("hevc_mp4toannexb"),&b[k]));CHECK(avcodec_parameters_copy(b[k]->par_in,s->streams[k]->codecpar));b[k]->time_base_in=s->streams[k]->time_base;CHECK(av_bsf_init(b[k]));c[k]=avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_HEVC));CHECK(avcodec_parameters_to_context(c[k],b[k]->par_out));c[k]->thread_count=1;CHECK(avcodec_open2(c[k],NULL,NULL));}
    while(av_read_frame(s,p)>=0){int k=p->stream_index;REQUIRE(seen[k]<12);expected[k][seen[k]++]=decode(b[k],c[k],p);av_packet_unref(p);}
    for(unsigned k=0;k<s->nb_streams;k++){REQUIRE(seen[k]==12);for(int n=0;n<12;n++)REQUIRE(expected[k][n].w==((n>=4&&n<8)?96:64));}
    int sequence[]={5,5,6,4,1,9,7,7,0,11,4,8};
    for(unsigned j=0;j<sizeof(sequence)/sizeof(*sequence);j++){
        int target=sequence[j];CHECK(av_seek_frame(s,0,av_rescale_q(target,(AVRational){1,1},s->streams[0]->time_base),AVSEEK_FLAG_BACKWARD));
        for(unsigned k=0;k<s->nb_streams;k++){av_bsf_flush(b[k]);avcodec_flush_buffers(c[k]);seen[k]=0;}
        for(unsigned k=0;k<s->nb_streams;k++){
            CHECK(av_read_frame(s,p));int track=p->stream_index;REQUIRE(!seen[track]++);size_t size=0;const unsigned char *side=av_packet_get_side_data(p,AV_PKT_DATA_NEW_EXTRADATA,&size);REQUIRE(side&&size>0);
            int n=av_rescale_q(p->pts,s->streams[track]->time_base,(AVRational){1,1});REQUIRE(n==target);Digest got=decode(b[track],c[track],p);REQUIRE(got.w==expected[track][n].w&&got.h==expected[track][n].h&&!memcmp(got.md5,expected[track][n].md5,16));checks++;av_packet_unref(p);
        }
    }
    printf("PASS seek_individually=%s streams=%u exact-YUV seek checks=%d\n",argv[2],s->nb_streams,checks);
    for(unsigned k=0;k<s->nb_streams;k++){av_bsf_free(&b[k]);avcodec_free_context(&c[k]);}avformat_close_input(&s);av_packet_free(&p);return 0;
}
