#!/usr/bin/env python3
"""Build a regular two-description MP4 from independently valid x265 IDR streams.
Each sample is its own chunk and its description is copied intact from its encoder
output. In-band parameter-set NALs are removed for hvc1 conformance; slice bytes and
hvcC syntax are never modified or mismatched.
"""
import pathlib, struct, subprocess, sys
D=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parent
D.mkdir(parents=True, exist_ok=True)
u=lambda x:struct.pack('>I',x)
def box(t,p):return u(len(p)+8)+t+p
def boxes(data):
    p=0
    while p<len(data):
        n=struct.unpack_from('>I',data,p)[0]; assert n>=8 and p+n<=len(data)
        yield data[p+4:p+8],data[p+8:p+n];p+=n
    assert p==len(data)
def child(data,t):return next(p for typ,p in boxes(data) if typ==t)
def get(data,*path):
    for t in path:data=child(data,t)
    return data
sources=[]
for i,size in enumerate(['64x64','96x64']):
    p=D/f'source-{i}.mp4'
    subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i',f'testsrc2=size={size}:rate=1','-frames:v','4','-c:v','libx265','-threads','1','-x265-params','pools=none:frame-threads=1:keyint=1:min-keyint=1:repeat-headers=0:log-level=error','-tag:v','hvc1','-video_track_timescale','1024',str(p)],check=True)
    data=p.read_bytes();stbl=get(data,b'moov',b'trak',b'mdia',b'minf',b'stbl')
    entry=get(stbl,b'stsd')[8:]
    hvcc=get(entry[86:],b'hvcC');assert (hvcc[21]&3)==3
    hsets=set();q=23
    for _ in range(hvcc[22]):
        count=struct.unpack_from('>H',hvcc,q+1)[0];q+=3
        for _ in range(count):
            length=struct.unpack_from('>H',hvcc,q)[0];q+=2
            hsets.add(hvcc[q:q+length]);q+=length
    assert q==len(hvcc)
    sz=get(stbl,b'stsz'); sizes=struct.unpack('>4I',sz[12:]); assert sz[4:8]==u(0)
    mdat=get(data,b'mdat');assert sum(sizes)==len(mdat)
    samples=[];off=0
    for n in sizes:
        sample=mdat[off:off+n];off+=n
        # hvc1 stores parameter sets exclusively in hvcC. Verify each picture
        # really is an independent IDR, not a CRA requiring earlier pictures.
        q=0; types=[]; kept=[]
        while q<len(sample):
            length=struct.unpack_from('>I',sample,q)[0];q+=4
            assert length>=2 and q+length<=len(sample)
            typ=(sample[q]>>1)&63;types.append(typ)
            if typ in (32,33,34):assert sample[q:q+length] in hsets
            else:kept.append(u(length)+sample[q:q+length])
            q+=length
        assert q==len(sample) and any(t in (19,20) for t in types)
        samples.append(b"".join(kept))

    sources.append((data,get(stbl,b'stsd')[8:],samples))
# Four samples for each description, with a return to description 1.
samples=sources[0][2]+sources[1][2]+sources[0][2]
ids=[1]*4+[2]*4+[1]*4
ftyp=box(b'ftyp',get(sources[0][0],b'ftyp'));offset=len(ftyp)+8
stbl=box(b'stsd',b'\0'*4+u(2)+sources[0][1]+sources[1][1])
stbl+=box(b'stts',b'\0'*4+u(1)+u(len(samples))+u(1024))
stbl+=box(b'stsc',b'\0'*4+u(3)+b''.join(u(first)+u(1)+u(idx) for first,idx in [(1,1),(5,2),(9,1)]))
stbl+=box(b'stsz',b'\0'*8+u(len(samples))+b''.join(u(len(s)) for s in samples))
offsets=[]
for s in samples:offsets.append(offset);offset+=len(s)
stbl+=box(b'stco',b'\0'*4+u(len(samples))+b''.join(u(o) for o in offsets))
# All encoder frames are IDR and independently decodable; absent stss means all sync.
def rewrite(data):
    out=b''
    for t,p in boxes(data):
        if t==b'stbl':p=stbl
        elif t in (b'moov',b'trak',b'mdia',b'minf'):p=rewrite(p)
        elif t==b'edts':continue
        elif t==b'mvhd':p=p[:16]+u(12000)+p[20:]
        elif t==b'tkhd':p=p[:20]+u(12000)+p[24:]
        elif t==b'mdhd':p=p[:16]+u(12288)+p[20:]
        out+=box(t,p)
    return out
out=ftyp+box(b'mdat',b''.join(samples))+rewrite(box(b'moov',get(sources[0][0],b'moov')))
(D/'two-descriptions.mp4').write_bytes(out)
print('Created two-descriptions.mp4: 12 IDR samples, description sequence 1/2/1')

# Duplicate the video track with a distinct track ID. Shared media bytes are
# legal; this exercises reselecting a non-target track on interleaved seeks.
moov=get(out,b'moov');parts=[]
for t,p in boxes(moov):
    if t==b'mvhd':p=p[:-4]+u(3)
    parts.append(box(t,p))
    if t==b'trak':
        twin=b''
        for tt,pp in boxes(p):
            if tt==b'tkhd':pp=pp[:12]+u(2)+pp[16:]
            twin+=box(tt,pp)
        parts.append(box(t,twin))
(D/'two-tracks.mp4').write_bytes(ftyp+box(b'mdat',b''.join(samples))+box(b'moov',b''.join(parts)))
