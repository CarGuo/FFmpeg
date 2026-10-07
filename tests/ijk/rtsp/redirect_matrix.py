#!/usr/bin/env python3
"""Ordinary RTSP 302 / endpoint switching regression cases on loopback only."""
import argparse, json, os, subprocess, sys, time
from pathlib import Path
from urllib.parse import urlsplit
from rtsp_fixture import RTSPFixture

class Fixture(RTSPFixture):
    def __init__(self, *a, redirect_method=None, target=None, media_id=255, route=None, **kw):
        super().__init__(*a, **kw)
        self.redirect_method, self.target, self.media_id, self.route = redirect_method, target, media_id, route
        self.requests = []
    def response_override(self, method, uri, headers):
        self.requests.append({'method': method, 'uri': uri, 'session': headers.get('session'),
                              'authorization_present': 'authorization' in headers, 'port': self.port})
        if self.route:
            result = self.route(self, method, uri, headers)
            if result: return result
        if method == self.redirect_method:
            time.sleep(.01)
            return '302 Moved Temporarily', {'Location': self.target}, b''
        return None
    def _stream(self, tracks, send, active):
        def identity_send(data):
            if len(data) > 16 and data[0] == 36 and data[5] & 127 == 0:
                data = data[:16] + bytes([self.media_id]) * (len(data)-16)
            send(data)
        return super()._stream(tracks, identity_send, active)

def main():
    p = argparse.ArgumentParser(); p.add_argument('--probe'); p.add_argument('--out', required=True); p.add_argument('--expect',choices=('baseline','patched')); a = p.parse_args()
    result=[]; fixtures=[]
    def new(**kw):
        f=Fixture(duration=2, **kw).start(); fixtures.append(f); return f
    def run(name, urls, servers, env=None, timeout=4):
        e=os.environ.copy(); e.update(env or {})
        if a.probe: cmd=[a.probe]+urls
        else:
            assert len(urls)==1
            cmd=['ffprobe','-v','warning','-rtsp_transport','tcp','-timeout','300000','-analyzeduration','1','-probesize','32',
                 '-show_packets','-show_entries','packet=size','-read_intervals','%+#3','-of','json',urls[0]]
        before=[len(s.requests) for s in servers]; start=time.monotonic()
        try:
            r=subprocess.run(cmd,env=e,capture_output=True,text=True,timeout=timeout)
            record={'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'timed_out':False}
        except subprocess.TimeoutExpired as exc:
            record={'returncode':None,'stdout':str(exc.stdout),'stderr':str(exc.stderr),'timed_out':True}
        record.update(name=name,urls=urls,elapsed_ms=round((time.monotonic()-start)*1000,2),
                      requests=[s.requests[n:] for s,n in zip(servers,before)])
        result.append(record)
        print(json.dumps({k:v for k,v in record.items() if k not in ('requests','stderr')}),flush=True)
    target=new(media_id=238); target.url=target.url.replace('/live','/camera-two')
    run('direct-target',[target.url],[target])
    for method in ('OPTIONS','DESCRIBE','SETUP','PLAY'):
        origin=new(redirect_method=method,target=target.url,media_id=170)
        run('redirect-'+method.lower(),[origin.url],[origin,target])
    host_target=new(bind_host='127.0.0.2',media_id=221)
    origin=new(redirect_method='DESCRIBE',target=host_target.url)
    run('redirect-host-port-path',[origin.url],[origin,host_target])
    def chain(f,method,uri,headers):
        if method=='OPTIONS' and '/hop' in uri:
            n=int(urlsplit(uri).path[4:])
            if n:
                return '302 Moved Temporarily', {'Location':f.url.rsplit('/',1)[0]+'/hop'+str(n-1)}, b''
    chain_server=new(route=chain)
    run('redirect-chain-three',[chain_server.url.replace('/live','/hop3')],[chain_server])
    run('redirect-chain-eight',[chain_server.url.replace('/live','/hop8')],[chain_server])
    run('redirect-chain-nine',[chain_server.url.replace('/live','/hop9')],[chain_server])
    loop=new(redirect_method='OPTIONS'); loop.target=loop.url
    run('redirect-loop',[loop.url],[loop],{'PROBE_DEADLINE_MS':'400'},timeout=1.2)
    relative=new(redirect_method='DESCRIBE',target='/camera-two')
    run('relative-location-compatibility',[relative.url],[relative])
    if a.probe:
        first=new(media_id=170); second=new(media_id=187)
        run('same-ip-two-ports-reopen',[first.url,second.url,first.url],[first,second])
        run('interrupt-then-reopen',[loop.url,target.url],[loop,target],{'PROBE_FIRST_DEADLINE_MS':'20','PROBE_DEADLINE_MS':'400'})
    Path(a.out).write_text(json.dumps(result,indent=2))
    if a.probe and a.expect:
        failed=[]
        for r in result:
            rows=[json.loads(line) for line in r['stdout'].splitlines()]
            name=r['name']; expected_error=False
            if name in ('redirect-setup','redirect-play') and a.expect=='baseline': expected_error=True
            if name in ('relative-location-compatibility','redirect-loop'): expected_error=True
            if name=='redirect-chain-nine' and a.expect=='patched':expected_error=True
            if name=='interrupt-then-reopen':
                assert rows[0]['open_ret']<0 and rows[1]['open_ret']==0 and rows[1]['packets']==3, r
                if a.expect=='patched':assert rows[0]['open_ret']==-1414092869,r
                continue
            if expected_error:
                assert rows[0]['open_ret']<0,r
                if a.expect=='patched' and name in ('redirect-loop','redirect-chain-nine'):
                    assert rows[0]['open_ret']==-40 and len(r['requests'][0])<=9,r
            else:
                assert all(x['open_ret']==0 and x['packets']==3 and x['streams']==1 for x in rows),r
                if name in ('direct-target','redirect-options','redirect-describe','redirect-setup','redirect-play'):
                    assert rows[0]['first_byte']==238,r
                if name=='same-ip-two-ports-reopen':assert [x['first_byte'] for x in rows]==[170,187,170],r
            if name.startswith('redirect-') and len(r['requests'])>1 and r['requests'][1]:
                assert r['requests'][1][0]['method']=='OPTIONS' and r['requests'][1][0]['session'] is None,r
        print('PASS asserted '+a.expect+' redirect matrix')
    for f in fixtures:f.close()

if __name__=='__main__':main()
