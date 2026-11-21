"""Build native RGB565 delta playback data from the approved preview (Pillow)."""
from PIL import Image, ImageSequence
from pathlib import Path
import numpy as np
import struct, base64, hashlib
import sys

def pack(a):
    out=bytearray();i=0
    while i<len(a):
        n=1
        while i+n<len(a) and a[i+n]==a[i] and n<128:n+=1
        if n>=3:out.extend((128+n-1,int(a[i])));i+=n;continue
        start=i;i+=n
        while i<len(a) and i-start<128:
            if i+2<len(a) and a[i]==a[i+1]==a[i+2]:break
            i+=1
        count=min(i-start,128);i=start+count
        out.append(count-1);out.extend(a[start:i])
    return out

def encode(source,destination):
    im=Image.open(source)
    frames=[f.convert('RGB').resize((360,360),Image.Resampling.LANCZOS) for i,f in enumerate(ImageSequence.Iterator(im)) if i%2==0]
    sheet=Image.new('RGB',(1440,360))
    for j,k in enumerate([10,30,55,80]):sheet.paste(frames[k],(360*j,0))
    pal=sheet.quantize(colors=128,method=Image.Quantize.MEDIANCUT)
    rgb=pal.getpalette();rgb += [0]*(768-len(rgb));palette=[]
    for i in range(256):
        r,g,b=rgb[i*3:i*3+3];palette.append(((r>>3)<<11)|((g>>2)<<5)|(b>>3))
    out=bytearray(struct.pack('<4sHHHHI',b'VPB1',360,360,len(frames),80,0))
    out.extend(struct.pack('<256H',*palette));prev=np.zeros((360,360),dtype=np.uint8)
    expected=[]
    for fi,f in enumerate(frames):
        a=np.array(f.quantize(palette=pal,dither=Image.Dither.NONE));body=bytearray();cursor=0
        for y in range(360):
            changed=np.flatnonzero(a[y]!=prev[y]) if fi else np.arange(360)
            if not len(changed):continue
            x0,x1=int(changed[0]),int(changed[-1])+1;pos=y*360+x0;skip=pos-cursor
            while skip>65535:body.extend(struct.pack('<HHH',65535,0,0));skip-=65535
            data=pack(a[y,x0:x1]);body.extend(struct.pack('<HHH',skip,x1-x0,len(data)));body.extend(data);cursor=pos+x1-x0
        out.extend(struct.pack('<I',len(body)));out.extend(body);prev=a
        native=np.repeat(np.repeat(np.array(palette,dtype='<u2')[a],2,axis=0),2,axis=1)
        expected.append(hashlib.sha256(native.tobytes()).hexdigest())
    dest=Path(destination);dest.write_bytes(out);dest.with_suffix('.vpb.b64').write_bytes(base64.encodebytes(out))
    Path('/tmp/boot_expected.txt').write_text('\n'.join(expected))
    print('Encoded',len(frames),'frames;',len(out),'bytes; SHA256',hashlib.sha256(out).hexdigest())
if __name__=='__main__':encode(*sys.argv[1:])
