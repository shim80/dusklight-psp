#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, sys, zlib
from PIL import Image, ImageDraw, ImageFont

def swizzle(raw,w,h):
    out=bytearray(); row=w*2
    for by in range(0,h,8):
        for bx in range(0,row,16):
            for y in range(8): out+=raw[(by+y)*row+bx:(by+y)*row+bx+16]
    return out

def pack4444(im):
    raw=bytearray(im.width*im.height*2)
    for y in range(im.height):
        for x in range(im.width):
            r,g,b,a=im.getpixel((x,y)); v=(r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12)
            struct.pack_into('<H',raw,(y*im.width+x)*2,v)
    return swizzle(raw,im.width,im.height)

def crc(blob):
    b=bytearray(blob); b[12:16]=b'\0'*4; return zlib.crc32(b)&0xffffffff

def main():
    if len(sys.argv)!=2: raise SystemExit('usage: startup_logo_ui_export.py <output.dpsu>')
    # Original, non-commercial team card. No Nintendo/Dolby/warning/progressive assets.
    atlas=Image.new('RGBA',(512,512),(0,0,0,0)); card=Image.new('RGBA',(320,96),(0,0,0,0))
    d=ImageDraw.Draw(card); font=ImageFont.load_default(size=28)
    text='DUSKLIGHT'; sub='PSP TEAM'
    box=d.textbbox((0,0),text,font=font); x=(320-(box[2]-box[0]))//2
    d.text((x,22),text,font=font,fill=(245,245,255,255),stroke_width=1,stroke_fill=(60,70,96,255))
    subfont=ImageFont.load_default(size=14); box=d.textbbox((0,0),sub,font=subfont); x=(320-(box[2]-box[0]))//2
    d.text((x,62),sub,font=subfont,fill=(180,190,215,255))
    atlas.alpha_composite(card,(0,0))
    count=1; atlas_off=128+32; total=atlas_off+512*512*2
    out=bytearray(total); out[:4]=b'DPSU'; struct.pack_into('<HHI',out,4,1,128,total)
    struct.pack_into('<IIIIIIII',out,16,512,512,2,count,128,32,atlas_off,512*512*2)
    # id=0, channel=0, centered 320x96 card from atlas origin.
    struct.pack_into('<HHhhHHHHHH',out,128,0,0,80,88,320,96,0,0,320,96)
    struct.pack_into('<I',out,152,0xffffffff)
    out[atlas_off:]=pack4444(atlas); struct.pack_into('<I',out,12,crc(out))
    p=Path(sys.argv[1]); p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(out)
    print(f'DUSKLIGHT_TEAM_LOGO_UI_OK records=1 channel=0 sha256={hashlib.sha256(out).hexdigest()}')
if __name__=='__main__': main()
