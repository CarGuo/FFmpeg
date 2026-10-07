#!/usr/bin/env python3
"""Deterministic RTSP timeouts and DNS changes; process injection only."""
import argparse, json, os, subprocess, time
from pathlib import Path
from redirect_matrix import Fixture
class Stall(Fixture):
    def __init__(self,*a,stall='read',**kw):super().__init__(*a,**kw);self.stall=stall
    def response_override(self,method,uri,headers):
        if method==self.stall:
            time.sleep(2)
        return super().response_override(method,uri,headers)
    def _stream(self,tracks,send,active):
        if self.stall=='read':time.sleep(2)
        else:super()._stream(tracks,send,active)

def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--out',required=True);p.add_argument('--expect',choices=('baseline','patched'),default='patched');a=p.parse_args()
    out=Path(a.out).resolve();out.parent.mkdir(parents=True,exist_ok=True)
    result=[];fixtures=[]
    def new(cls=Fixture,**kw):
        f=cls(duration=2,**kw).start();fixtures.append(f);return f
    def run(name,urls,servers,extra=None,change=None):
        env=os.environ.copy();env.update(extra or {})
        n=[len(s.requests) for s in servers];t=time.monotonic()
        proc=subprocess.Popen([a.probe]+urls,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
        first=''
        if change:
            first=proc.stdout.readline();change()
        stdout,stderr=proc.communicate(timeout=5)
        r=dict(name=name,stdout=first+stdout,stderr=stderr,returncode=proc.returncode,
               elapsed_ms=round((time.monotonic()-t)*1000,2),requests=[s.requests[i:] for s,i in zip(servers,n)])
        result.append(r);print(json.dumps({k:v for k,v in r.items() if k!='requests'}),flush=True)
    good=new(media_id=187)
    for phase in ('OPTIONS','read'):
        f=new(Stall,stall=phase)
        run('positive-timeout-'+phase,[f.url,good.url],[f,good],{'PROBE_SOCKET_TIMEOUT':'150000','PROBE_DEADLINE_MS':'800'})
        for infinite in ('0','-1'):
            run('interrupt-timeout-'+infinite+'-'+phase,[f.url,good.url],[f,good],{'PROBE_SOCKET_TIMEOUT':infinite,'PROBE_DEADLINE_MS':'350'})
    mapfile=out.with_suffix('.dns-map');mapfile.write_text('127.0.0.1\n')
    shim=Path(__file__).with_name('fixture_resolver.so').resolve()
    env={'FIXTURE_DNS_MAP':str(mapfile),'LD_PRELOAD':str(shim),'PROBE_BETWEEN_OPENS_MS':'100'}
    first=new(media_id=170);second=new(bind_host='127.0.0.2',port=first.port,media_id=204)
    url=first.url.replace('127.0.0.1','fixture.invalid')
    run('dns-change-next-open',[url,url],[first,second],env,lambda:mapfile.write_text('127.0.0.2\n'))
    mapfile.write_text('127.0.0.3,127.0.0.2\n')
    run('dns-address-list-fallback',[url],[first,second],env)
    mapfile.write_text('127.0.0.3\n')
    run('dns-stale-answer-then-next-open',[url,url],[first,second],env,lambda:mapfile.write_text('127.0.0.2\n'))
    out.write_text(json.dumps(result,indent=2))
    for r in result:
        rows=[json.loads(x) for x in r['stdout'].splitlines()];name=r['name']
        if 'timeout' in name:
            first=rows[0]
            if name.startswith('positive'):
                expected=-1094995529 if a.expect=='baseline' and name.endswith('OPTIONS') else -110
                assert (first['open_ret'] if name.endswith('OPTIONS') else first['read_ret'])==expected,r
                assert 120<=first['elapsed_ms']<750,r
            else:
                expected=-1094995529 if a.expect=='baseline' and name.endswith('OPTIONS') else -1414092869
                assert (first['open_ret'] if name.endswith('OPTIONS') else first['read_ret'])==expected,r
                assert 300<=first['elapsed_ms']<750,r
            assert rows[1]['open_ret']==0 and rows[1]['packets']==3 and rows[1]['first_byte']==187,r
        elif name=='dns-change-next-open':assert [x['first_byte'] for x in rows]==[170,204],r
        elif name=='dns-address-list-fallback':assert rows[0]['first_byte']==204 and rows[0]['packets']==3,r
        elif name=='dns-stale-answer-then-next-open':assert rows[0]['open_ret']==-111 and rows[1]['first_byte']==204,r
    print('PASS asserted '+a.expect+' timeout/DNS matrix')
    for f in fixtures:f.close()
if __name__=='__main__':main()
