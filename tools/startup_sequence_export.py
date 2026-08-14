#!/usr/bin/env python3
from pathlib import Path
import struct, sys, zlib

HEADER=64
STRIDE=32
CRC_OFF=28
# segment, policy, completeness, duration, fade in, fade out, caps, source token
# Visual flow: Dusklight team logo -> F_SP102 title -> START -> file/save flow.
SEGMENTS=[
    (0,1,0,150,18,18,1,0x4455534B),       # TeamLogo, TimedOrInput, UI
    (4,3,0,0,0,0,2|4|16,0),               # OpeningLoad only initializes title assets
    (6,4,0,0,0,0,2|4|16,7),               # TitleLogo source event / DPAN clip 7
    (7,2,0,0,0,0,1|2|4|16,0),             # TitlePrompt requires START
    (8,2,0,0,0,0,1|32,0),                 # FileSelect handed to canonical game/save flow
]

def crc(blob):
    b=bytearray(blob); b[CRC_OFF:CRC_OFF+4]=b'\0'*4
    return zlib.crc32(b)&0xffffffff

def build():
    out=bytearray(HEADER+len(SEGMENTS)*STRIDE)
    out[:4]=b'DPST'
    struct.pack_into('<HHIIII',out,4,1,HEADER,len(out),len(SEGMENTS),HEADER,STRIDE)
    for i,(seg,policy,complete,duration,fade_in,fade_out,caps,token) in enumerate(SEGMENTS):
        off=HEADER+i*STRIDE
        struct.pack_into('<BBBBIIIII',out,off,seg,policy,complete,0,duration,fade_in,fade_out,caps,token)
    struct.pack_into('<I',out,CRC_OFF,crc(out))
    return out

def main():
    if len(sys.argv)!=2: raise SystemExit('usage: startup_sequence_export.py <output.dpst>')
    out=build(); p=Path(sys.argv[1]); p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(out)
    print('DUSKLIGHT_STARTUP_SEQUENCE_OK segments=5 flow=team_logo,title,start,file_select')
if __name__=='__main__': main()
