#!/usr/bin/env python3
import argparse,json,os,subprocess
from pathlib import Path
from redirect_matrix import Fixture
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--out',required=True);a=p.parse_args();results=[]
target=Fixture(media_id=231).start()
def route(f,method,uri,headers):
    if method=='DESCRIBE':return '200 OK',{'Content-Type':'application/sdp','Content-Base':target.url+'/'},f.sdp()
    if method=='SETUP':return '404 Not Found',{},b''
origin=Fixture(route=route).start()
def run(name,url,env):
    e=os.environ.copy();e.update(env)
    r=subprocess.run([a.probe,url],env=e,capture_output=True,text=True,timeout=3)
    result=dict(name=name,rows=[json.loads(x) for x in r.stdout.splitlines()],stderr=r.stderr)
    results.append(result);return result
r=run('cross-port-content-base-no-redirect',origin.url,{})
assert r['rows'][0]['open_ret']<0 and not target.requests,r
assert any(q['method']=='SETUP' and q['uri'].startswith(target.url) for q in origin.requests),origin.requests
r['origin_requests']=list(origin.requests);r['target_requests']=list(target.requests)
# A gateway can intentionally serve an absolute URI on its existing socket.
def proxy_route(f,method,uri,headers):
    if method=='DESCRIBE':return '200 OK',{'Content-Type':'application/sdp','Content-Base':target.url+'/'},f.sdp()
proxy=Fixture(route=proxy_route,media_id=232).start()
r=run('cross-port-base-gateway-control',proxy.url,{})
assert r['rows'][0]['packets']==3 and r['rows'][0]['first_byte']==232 and not target.requests,r
r['gateway_requests']=list(proxy.requests);r['target_requests']=list(target.requests)
proxy.close()
mapfile=Path(a.out).resolve().with_suffix('.dns-map');mapfile.write_text('127.0.0.1\n')
env={'LD_PRELOAD':str(Path(__file__).with_name('fixture_resolver.so').resolve()),'FIXTURE_DNS_MAP':str(mapfile),'FIXTURE_DNS_DELAY_MS':'500','PROBE_DEADLINE_MS':'150','PROBE_PACKETS':'1'}
r=run('delayed-synchronous-resolver',target.url.replace('127.0.0.1','fixture.invalid'),env)
assert r['rows'][0]['open_ret']<0 and r['rows'][0]['elapsed_ms']>=500,r
r=run('literal-IP-interrupt-control',target.url,env)
assert r['rows'][0]['open_ret']==0 and r['rows'][0]['first_byte']==231,r
Path(a.out).write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2));origin.close();target.close()
