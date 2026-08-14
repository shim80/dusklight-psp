#!/usr/bin/env python3
from pathlib import Path
import dataclasses, hashlib, os, struct, sys, types, typing, zlib
from PIL import Image
if not hasattr(dataclasses,'_recursive_repr') and hasattr(dataclasses,'recursive_repr'): dataclasses._recursive_repr=dataclasses.recursive_repr
G=Path(os.environ.get('GCLIB_ROOT',''))
if not G.is_dir(): raise SystemExit('GCLIB_ROOT must point to the pinned gclib checkout')
sys.path.insert(0,str(G))
import gclib.bunfoe as _b
from gclib import fs_helpers as _fs
from gclib.fs_helpers import FixedStr,MagicStr
_o1=_b.BUNFOE.get_byte_size; _o2=_b.BUNFOE.read_value
def _g(t):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr): return typing.get_args(t)[0]
    return _o1(t)
def _r(self,t,o):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr): return _fs.read_str(self.data,o,typing.get_args(t)[0])
    return _o2(self,t,o)
_b.BUNFOE.get_byte_size=staticmethod(_g); _b.BUNFOE.read_value=_r
try: import imagequant
except ImportError:
    m=types.ModuleType('imagequant');m.quantize_pil_image=lambda *a,**k:None;sys.modules['imagequant']=m
from gclib.bti import BTI

def align(v,n=8): return (v+n-1)&~(n-1)
def load_half(path):
    im=BTI(str(path)).render().convert('RGBA')
    if im.width%2 or im.height%2: raise ValueError((path,'odd 2x dimensions'))
    return im.resize((im.width//2,im.height//2),Image.Resampling.LANCZOS)
def swizzle(raw,w,h):
    if w%8 or h%8: raise ValueError('atlas must be 8-pixel aligned')
    out=bytearray(); row=w*2
    for by in range(0,h,8):
      for bx in range(0,row,16):
        for y in range(8): out+=raw[(by+y)*row+bx:(by+y)*row+bx+16]
    return out
def pack4444(im):
    raw=bytearray(im.width*im.height*2)
    for y in range(im.height):
      for x in range(im.width):
        r,g,b,a=im.getpixel((x,y));v=(r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12);struct.pack_into('<H',raw,(y*im.width+x)*2,v)
    return swizzle(raw,im.width,im.height)
def crc(blob):
    b=bytearray(blob);b[12:16]=b'\0'*4;return zlib.crc32(b)&0xffffffff

def main():
    if len(sys.argv)!=8: raise SystemExit('usage: startup_logo_ui_export.py <warning> <warning_prompt> <nintendo> <dolby> <progressive_inter> <progressive_pro> <output.dpsu>')
    paths=[Path(x) for x in sys.argv[1:7]]; output=Path(sys.argv[7])
    specs=[(0,0),(1,0),(2,1),(3,2),(4,3),(5,3)]
    ims=[load_half(p) for p in paths]
    atlas=Image.new('RGBA',(512,512),(0,0,0,0)); positions=[];x=y=rowh=0
    for im in ims:
      if x+im.width>512:x=0;y+=rowh;rowh=0
      if y+im.height>512:raise ValueError('DPSU atlas overflow')
      atlas.alpha_composite(im,(x,y));positions.append((x,y));x+=align(im.width);rowh=max(rowh,align(im.height))
    records=[]
    for idx,((ident,ch),im,(u,v)) in enumerate(zip(specs,ims,positions)):
      if idx==0: dx,dy=(480-im.width)//2,(272-im.height)//2
      elif idx==1: dx,dy=(480-im.width)//2,225
      elif idx in (4,5): dx,dy=(480-im.width)//2,(104 if idx==4 else 148)
      else: dx,dy=(480-im.width)//2,(272-im.height)//2
      records.append((ident,ch,dx,dy,im.width,im.height,u,v,im.width,im.height))
    count=len(records);atlas_off=128+count*32;total=atlas_off+512*512*2
    out=bytearray(total);out[:4]=b'DPSU';struct.pack_into('<HHI',out,4,1,128,total);struct.pack_into('<IIIIIIII',out,16,512,512,2,count,128,32,atlas_off,512*512*2)
    for n,r in enumerate(records):
      ident,ch,dx,dy,w,h,u,v,sw,sh=r;off=128+n*32;struct.pack_into('<HHhhHHHHHH',out,off,ident,ch,dx,dy,w,h,u,v,sw,sh);struct.pack_into('<I',out,off+24,0xffffffff)
    out[atlas_off:]=pack4444(atlas);struct.pack_into('<I',out,12,crc(out));output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(out)
    print(f'DUSKLIGHT_STARTUP_LOGO_UI_EXPORT_OK records={count} atlas=512x512 bytes={len(out)} sha256={hashlib.sha256(out).hexdigest()}')
if __name__=='__main__':main()
