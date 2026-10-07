#!/usr/bin/env python3
import argparse,json,os,subprocess,time,threading
from pathlib import Path
from redirect_matrix import Fixture
class Ranged(Fixture):
    def sdp(self):return super().sdp().replace(b'a=control:*',b'a=range:npt=10.000-70.000\r\na=control:*')
class TwoTracks(Fixture):
    def sdp(self):return super().sdp()+b'm=audio 0 RTP/AVP 8\r\na=rtpmap:8 PCMA/8000/1\r\na=control:trackID=1\r\n'
class BlockUDP(Fixture):
    def _stream(self,tracks,send,active):
        if any(t[0]=='udp' for t in tracks.values()):active.clear();return
        return super()._stream(tracks,send,active)
class Disconnect(Fixture):
    def _stream(self,tracks,send,active):
        self.duration=.06
        super()._stream(tracks,send,active)
        self.close()

def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--out',required=True);a=p.parse_args();results=[];fixtures=[]
    def new(cls=Fixture,**kw):
        f=cls(duration=2,**kw).start();fixtures.append(f);return f
    def run(name,urls,servers,extra=None,change=None):
        env=os.environ.copy();env.update(extra or {});lens=[len(s.requests) for s in servers]
        timer=threading.Timer(.1,change) if change else None
        if timer:timer.start()
        r=subprocess.run([a.probe]+urls,env=env,capture_output=True,text=True,timeout=5)
        if timer:timer.join()
        rows=[json.loads(x) for x in r.stdout.splitlines()]
        record=dict(name=name,rows=rows,stderr=r.stderr,requests=[s.requests[n:] for s,n in zip(servers,lens)])
        results.append(record);print(json.dumps({'name':name,'rows':rows}),flush=True)
        return record
    live=new(media_id=201)
    for method in ('SETUP','PLAY'):
        origin=new(Ranged,redirect_method=method,target=live.url)
        r=run('recording-to-live-'+method,[origin.url],[origin,live]);row=r['rows'][0]
        assert row['open_ret']==0 and row['packets']==3 and row['duration']==-9223372036854775808 and row['first_byte']==201,r
    recording=new(Ranged,media_id=202);origin=new(redirect_method='PLAY',target=recording.url)
    r=run('live-to-recording-PLAY',[origin.url],[origin,recording]);assert r['rows'][0]['duration']==60000000 and r['rows'][0]['packets']==3,r
    def second_track(f,method,uri,headers):
        if method=='SETUP' and uri.endswith('trackID=1'):return '302 Moved Temporarily',{'Location':live.url},b''
    origin=new(TwoTracks,route=second_track)
    r=run('redirect-after-partial-setup',[origin.url],[origin,live]);assert r['rows'][0]['streams']==1 and r['rows'][0]['first_byte']==201,r
    middle=new(redirect_method='PLAY',target=live.url);origin=new(redirect_method='SETUP',target=middle.url)
    r=run('mixed-setup-play-chain',[origin.url],[origin,middle,live]);assert r['rows'][0]['packets']==3 and r['rows'][0]['streams']==1,r
    f=new(BlockUDP,media_id=203)
    r=run('udp-silent-fallback-tcp',[f.url],[f],{'PROBE_TRANSPORT':'udp+tcp','PROBE_SOCKET_TIMEOUT':'200000','PROBE_DEADLINE_MS':'1500'})
    assert r['rows'][0]['packets']==3 and r['rows'][0]['first_byte']==203,r
    assert sum(q['method']=='SETUP' for q in r['requests'][0])==2,r
    r=run('udp-silent-zero-timeout-interrupt',[f.url],[f],{'PROBE_TRANSPORT':'udp+tcp','PROBE_SOCKET_TIMEOUT':'0','PROBE_DEADLINE_MS':'350'})
    assert r['rows'][0]['packets']==0 and r['rows'][0]['read_ret']<0,r
    assert sum(q['method']=='SETUP' for q in r['requests'][0])==1,r
    # DNS changes alone do not move an established TCP session. Reopening does.
    mapfile=Path(a.out).resolve().with_suffix('.dns-map');mapfile.write_text('127.0.0.1\n')
    shim=Path(__file__).with_name('fixture_resolver.so').resolve()
    dns={'LD_PRELOAD':str(shim),'FIXTURE_DNS_MAP':str(mapfile),'PROBE_PACKETS':'20','PROBE_DEADLINE_MS':'1200'}
    first=new(media_id=170);second=new(bind_host='127.0.0.2',port=first.port,media_id=204)
    url=first.url.replace('127.0.0.1','fixture.invalid')
    r=run('active-session-dns-change-next-open',[url,url],[first,second],dns,lambda:mapfile.write_text('127.0.0.2\n'))
    assert [x['first_byte'] for x in r['rows']]==[170,204] and all(x['packets']==20 for x in r['rows']),r
    mapfile.write_text('127.0.0.1\n')
    first=new(Disconnect,media_id=171);second=new(bind_host='127.0.0.2',port=first.port,media_id=205)
    url=first.url.replace('127.0.0.1','fixture.invalid')
    r=run('active-disconnect-no-automatic-reopen',[url,url],[first,second],dns,lambda:mapfile.write_text('127.0.0.2\n'))
    assert r['rows'][0]['packets']<20 and r['rows'][0]['read_ret']<0 and r['rows'][0]['first_byte']==171,r
    assert r['rows'][1]['packets']==20 and r['rows'][1]['first_byte']==205,r
    Path(a.out).write_text(json.dumps(results,indent=2))
    for f in fixtures:f.close()
    print('PASS advanced RTSP timing, partial setup cleanup, mixed redirects, UDP fallback and active DNS/disconnect matrix')
if __name__=='__main__':main()
