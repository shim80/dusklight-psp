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
_o1=_b.BUNFOE.get_byte_size;_o2=_b.BUNFOE.read_value
def _g(t):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr):return typing.get_args(t)[0]
    return _o1(t)
def _r(self,t,o):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr):return _fs.read_str(self.data,o,typing.get_args(t)[0])
    return _o2(self,t,o)
_b.BUNFOE.get_byte_size=staticmethod(_g);_b.BUNFOE.read_value=_r
try:import imagequant
except ImportError:
    m=types.ModuleType('imagequant');m.quantize_pil_image=lambda *a,**k:None;sys.modules['imagequant']=m
from gclib.bfn import BFN

def swizzle(raw,w,h):
    out=bytearray();row=w*2
    for by in range(0,h,8):
      for bx in range(0,row,16):
       for y in range(8):out+=raw[(by+y)*row+bx:(by+y)*row+bx+16]
    return out
def crc(blob):
    b=bytearray(blob);b[12:16]=b'\0'*4;return zlib.crc32(b)&0xffffffff

def main():
    if len(sys.argv)!=4: raise SystemExit('usage: title_prompt_ui_export.py <rodan_b_24_22.bfn> <message> <output.dpsu>')
    font=BFN(sys.argv[1]);base=font.render_string(sys.argv[2],600).convert('RGBA');box=base.getbbox()
    if box is None:raise ValueError('empty title prompt')
    base=base.crop(box);scale=272/448;base=base.resize((max(1,round(base.width*scale)),max(1,round(base.height*scale))),Image.Resampling.LANCZOS)
    sprite=Image.new('RGBA',(base.width+6,base.height+6),(0,0,0,0));alpha=base.getchannel('A');shadow=Image.new('RGBA',base.size,(0,0,0,210));shadow.putalpha(alpha)
    for dx,dy in ((1,1),(2,1),(1,2)):sprite.alpha_composite(shadow,(dx+1,dy+1))
    white=Image.new('RGBA',base.size,(255,255,255,255));white.putalpha(alpha);sprite.alpha_composite(white,(1,1))
    atlas=Image.new('RGBA',(512,512),(0,0,0,0));atlas.alpha_composite(sprite,(0,0));raw=bytearray(512*512*2)
    for y in range(512):
      for x in range(512):
       r,g,b,a=atlas.getpixel((x,y));struct.pack_into('<H',raw,(y*512+x)*2,(r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12))
    packed=swizzle(raw,512,512);ao=160;out=bytearray(ao+len(packed));out[:4]=b'DPSU';struct.pack_into('<HHI',out,4,1,128,len(out));struct.pack_into('<IIIIIIII',out,16,512,512,2,1,128,32,ao,len(packed))
    # zelda_press_start.blo: n_all anchor is 304,352 on the 604x448 source layout.
    cx=round(304*480/604);cy=round(352*272/448);dx=cx-sprite.width//2;dy=cy-sprite.height//2
    struct.pack_into('<HHhhHHHHHH',out,128,0,8,dx,dy,sprite.width,sprite.height,0,0,sprite.width,sprite.height);struct.pack_into('<I',out,152,0xffffffff);struct.pack_into('<I',out,12,crc(out));p=Path(sys.argv[3]);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(out)
    print(f'DUSKLIGHT_TITLE_PROMPT_UI_EXPORT_OK channel=8 anchor=304,352 source=604x448 bytes={len(out)} sha256={hashlib.sha256(out).hexdigest()}')
if __name__=='__main__':main()
