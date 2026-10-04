#!/usr/bin/env python3
"""Extract exact production helpers; exercise guards and allocation failure."""
import pathlib,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
source=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else (D/'../../source/FFmpeg').resolve()
if len(sys.argv)>2:D=pathlib.Path(sys.argv[2]).resolve()
s=(source/'libavformat/mov.c').read_text()
def fn(name):
    start=s.rfind('\nstatic ',0,s.index(name+'('))+1
    p=s.index('{',start);depth=1;q=p+1
    while depth:
        depth+=(s[q]=='{')-(s[q]=='}');q+=1
    return s[start:q]+'\n'
pre='''#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define av_assert0 assert
#define AVERROR(x) (-(x))
#define AV_CODEC_ID_HEVC 173
#define AV_PKT_DATA_NEW_EXTRADATA 1
typedef struct {unsigned first,count,id;} Entry;
typedef struct {unsigned chunk_count,stsc_count,stsc_index;int stsc_sample,stsd_count,current_sample,last_stsd_index;Entry *stsc_data;int *extradata_size;uint8_t **extradata;} MOVStreamContext;
typedef struct {int codec_id;} Params;
typedef struct {int nb_index_entries;MOVStreamContext *priv_data;Params *codecpar;} AVStream;
#define ffstream(s) (s)
typedef struct {uint8_t data[64];} AVPacket;
static int fail_alloc;
static uint8_t *av_packet_new_side_data(AVPacket *p,int t,int n){return fail_alloc?NULL:p->data;}
'''
post='''int main(void){
Entry entries[]={{1,1,1},{5,1,2},{9,1,1}};
int sizes[]={3,3};uint8_t a[]={1,2,3},b[]={4,5,6};uint8_t *extra[]={a,b};
MOVStreamContext valid={.chunk_count=12,.stsc_count=3,.stsc_index=2,.stsc_sample=2,.stsd_count=2,.current_sample=5,.last_stsd_index=1,.stsc_data=entries,.extradata_size=sizes,.extradata=extra};
Params par={AV_CODEC_ID_HEVC};MOVStreamContext sc=valid;AVStream st={12,&sc,&par};int count=0;
#define GUARD(change) do {sc=valid;par.codec_id=AV_CODEC_ID_HEVC;change;mov_seek_hevc_extradata(&st,0);assert(sc.last_stsd_index==1);assert(sc.stsc_index==2);count++;}while(0)
GUARD(par.codec_id=27);GUARD(sc.stsd_count=1);GUARD(sc.chunk_count=0);GUARD(sc.stsc_count=0);GUARD(sc.stsc_data=NULL);GUARD(sc.current_sample=-1);GUARD(sc.current_sample=12);GUARD(sc.current_sample=20);
sc=valid;mov_seek_hevc_extradata(&st,0);assert(sc.last_stsd_index==-1&&sc.stsc_index==1&&sc.stsc_sample==1);count++;
sc=valid;mov_seek_hevc_extradata(&st,1);assert(sc.last_stsd_index==-1&&sc.stsc_index==2&&sc.stsc_sample==2);count++;
sc=valid;sc.stsc_index=1;sc.last_stsd_index=0;AVPacket pkt;fail_alloc=1;assert(mov_change_extradata(&sc,&pkt)==-ENOMEM&&sc.last_stsd_index==0);count++;
fail_alloc=0;assert(mov_change_extradata(&sc,&pkt)==0&&sc.last_stsd_index==1&&!memcmp(pkt.data,b,3));count++;
printf("PASS %d helper guard/reselection/allocation checks\\n",count);return 0;
}
'''
(D/'guards.generated.c').write_text(pre+''.join(fn(n) for n in ['mov_stsc_index_valid','mov_get_stsc_samples','mov_seek_stsc','mov_seek_hevc_extradata','mov_change_extradata'])+post)
subprocess.run(['cc','-g','-fsanitize=address,undefined','-o',str(D/'test_guards'),str(D/'guards.generated.c')],check=True)
subprocess.run([str(D/'test_guards')],check=True)
