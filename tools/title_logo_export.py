#!/usr/bin/env python3
from pathlib import Path
import dataclasses, hashlib, math, os, struct, sys, types, typing, zlib
if not hasattr(dataclasses,'_recursive_repr') and hasattr(dataclasses,'recursive_repr'): dataclasses._recursive_repr=dataclasses.recursive_repr
G=Path(os.environ.get('GCLIB_ROOT',''))
if not G.is_dir(): raise SystemExit('GCLIB_ROOT must point to the pinned gclib source checkout')
sys.path.insert(0,str(G))
import gclib.bunfoe as _bunfoe
from gclib import fs_helpers as _fs
from gclib.fs_helpers import FixedStr, MagicStr
_o1=_bunfoe.BUNFOE.get_byte_size; _o2=_bunfoe.BUNFOE.read_value
def _g(t):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr): return typing.get_args(t)[0]
    return _o1(t)
def _r(self,t,o):
    if isinstance(t,types.GenericAlias) and t.__origin__ in (FixedStr,MagicStr): return _fs.read_str(self.data,o,typing.get_args(t)[0])
    return _o2(self,t,o)
_bunfoe.BUNFOE.get_byte_size=staticmethod(_g); _bunfoe.BUNFOE.read_value=_r
try: import imagequant
except ImportError:
    m=types.ModuleType('imagequant'); m.quantize_pil_image=lambda *a,**k: (_ for _ in ()).throw(RuntimeError()); sys.modules['imagequant']=m
from gclib.j3d import BMD
from gclib.j3d_chunks.inf1 import INF1NodeType
from gclib import gx_enums as GX
from PIL import Image

def align(v,n=16): return (v+n-1)&~(n-1)
def crc(blob):
    b=bytearray(blob); b[12:16]=b'\0'*4; return zlib.crc32(b)&0xffffffff

def swizzle16(raw,w,h):
    row=w*2; out=bytearray()
    for by in range(0,h,8):
      for bx in range(0,row,16):
       for y in range(8): out += raw[(by+y)*row+bx:(by+y)*row+bx+16]
    return bytes(out)
def pack_image(img,max_dim=256):
    img=img.convert('RGBA')
    if img.width>max_dim or img.height>max_dim:
      s=min(max_dim/img.width,max_dim/img.height); img=img.resize((max(1,round(img.width*s)),max(1,round(img.height*s))),Image.Resampling.LANCZOS)
    w,h=img.size; sw=align(w,8); sh=align(h,8); pix=list(img.get_flattened_data()) if hasattr(img,'get_flattened_data') else list(img.getdata()); al={a for *_,a in pix}; fmt=0 if al=={255} else 1 if al.issubset({0,255}) else 2
    raw=bytearray(sw*sh*2)
    for y in range(h):
      for x in range(w):
       r,g,b,a=img.getpixel((x,y))
       v=(r>>3)|((g>>2)<<5)|((b>>3)<<11) if fmt==0 else ((r>>3)|((g>>3)<<5)|((b>>3)<<10)|((1 if a>=128 else 0)<<15) if fmt==1 else (r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12))
       struct.pack_into('<H',raw,(y*sw+x)*2,v)
    return w,h,sw,sh,fmt,swizzle16(bytes(raw),sw,sh)
def mat_class(m):
    ac=m.alpha_compare; bl=m.blend_mode
    if m.pixel_engine_mode==GX.PixelEngineMode.Alpha_Test or not(ac.comp0==GX.CompareType.Always and ac.comp1==GX.CompareType.Always): return 1
    if m.pixel_engine_mode==GX.PixelEngineMode.Opaque and bl.mode==GX.BlendMode.None_: return 0
    return 2
def parse_desc(shp,shape):
    buf=shp.data.getbuffer(); off=shp.attribute_table_offset+shape.first_attribute_offset; out=[]
    while True:
      a,t=struct.unpack_from('>II',buf,off); off+=8
      if a==0xff: break
      if t!=3: raise ValueError(('index type',a,t))
      out.append(a)
    return out
def strips(shp,shape):
    buf=shp.data.getbuffer(); attrs=parse_desc(shp,shape)
    for gi in range(shape.matrix_group_count):
      go=shp.mtx_group_table_offset+(shape.first_matrix_group_index+gi)*8; size,po=struct.unpack_from('>II',buf,go); q=shp.primitive_data_offset+po; end=q+size
      while q<end:
       cmd=buf[q]; q+=1
       if cmd==0: break
       if cmd!=0x98: raise ValueError(('primitive',hex(cmd)))
       count=struct.unpack_from('>H',buf,q)[0]; q+=2; st=[]
       for _ in range(count):
        d={}
        for a in attrs: d[a]=struct.unpack_from('>H',buf,q)[0]; q+=2
        st.append(d)
       yield st
def c32(rgba): r,g,b,a=rgba; return r|(g<<8)|(b<<16)|(a<<24)

def export_model(bmd_path,out):
    b=BMD(str(bmd_path)); mapping={}; cur=None
    for n in b.inf1.flat_hierarchy:
      if n.type==INF1NodeType.MATERIAL: cur=n.index
      elif n.type==INF1NodeType.SHAPE: mapping[n.index]=cur
    textures=[]; texmap={}
    def add(img):
      p=pack_image(img); k=(p[0],p[1],p[4],hashlib.sha256(p[5]).digest())
      if k not in texmap: texmap[k]=len(textures); textures.append(p)
      return texmap[k]
    white=add(Image.new('RGBA',(8,8),(255,255,255,255)))
    source_tex={}
    for m in b.mat3.materials:
      ti=next((x for x in m.textures if x is not None),None)
      if ti is not None and ti not in source_tex: source_tex[ti]=add(b.tex1.textures[ti].render())
    attrs=b.vtx1.attributes; vertices=[]; indices=[]; subs=[]; vmap={}; bounds=[float('inf')]*3+[-float('inf')]*3
    for si,shape in enumerate(b.shp1.shapes):
      mi=mapping[si]; m=b.mat3.materials[mi]; ti=next((x for x in m.textures if x is not None),None); tex=source_tex.get(ti,white); bucket=mat_class(m); first=len(indices)
      for st in strips(b.shp1,shape):
       ids=[]
       for d in st:
        pi=d[GX.Attr.Position.value]; ci=d.get(GX.Attr.Color0.value); ui=d.get(GX.Attr.Tex0.value); p=attrs[GX.Attr.Position][pi]
        if ci is not None:
          cf=attrs[GX.Attr.Color0][ci]; rgba=tuple(max(0,min(255,round(v*255))) for v in cf)
        else:
          col=m.material_colors[0]; rgba=(col.r,col.g,col.b,col.a) if col else (255,255,255,255)
        uv=attrs[GX.Attr.Tex0][ui] if ui is not None else (0.,0.); key=(pi,ci if ci is not None else -1,ui if ui is not None else -1)
        if key not in vmap:
          vmap[key]=len(vertices); vertices.append((float(uv[0]),float(uv[1]),c32(rgba),float(p[0]),float(p[1]),float(p[2])))
          for a in range(3): bounds[a]=min(bounds[a],p[a]); bounds[a+3]=max(bounds[a+3],p[a])
        ids.append(vmap[key])
       for i in range(2,len(ids)):
        tri=(ids[i-2],ids[i-1],ids[i]) if i%2==0 else (ids[i-1],ids[i-2],ids[i])
        if len(set(tri))==3: indices.extend(tri)
      count=len(indices)-first; flags=(1 if m.z_mode.depth_test else 0)|(2 if m.z_mode.depth_write else 0)
      subs.append((first,count,mi,tex,bucket,si,mi,flags,m.z_mode.depth_func.value))
    if len(vertices)!=24 or len(subs)!=6: raise ValueError(('title logo contract',len(vertices),len(subs)))
    head=bytearray(256); head[:4]=b'DPRM'; struct.pack_into('<HH',head,4,1,256); struct.pack_into('<IIIIIII',head,16,4,len(vertices),len(indices),len(indices)//3,len(subs),len(b.mat3.materials),len(textures)); struct.pack_into('<6f',head,48,*bounds); struct.pack_into('<I',head,72,256); struct.pack_into('<IIIIIII',head,80,24,0,4,8,12,16,20)
    sections=bytearray(128); cur=align(384,16); vo=cur;cur+=len(vertices)*24;cur=align(cur,16);io=cur;cur+=len(indices)*2;cur=align(cur,16);so=cur;cur+=len(subs)*48
    for i,(typ,off,cnt,stride) in enumerate([(1,vo,len(vertices),24),(2,io,len(indices),2),(3,so,len(subs),48),(4,0,len(b.mat3.materials),0)]): struct.pack_into('<IIIII',sections,i*32,typ,off,0,cnt,stride)
    d=bytearray(cur); d[:256]=head; d[256:384]=sections; q=vo
    for v in vertices: struct.pack_into('<ffIfff',d,q,*v);q+=24
    q=io
    for ix in indices:struct.pack_into('<H',d,q,ix);q+=2
    q=so
    for sm in subs:
      first,count,mat,tex,buck,shape,src,flags,df=sm
      struct.pack_into('<IIHHBBHHBB',d,q,first,count,mat,tex,buck,0,shape,src,flags,df);q+=48
    struct.pack_into('<I',d,8,len(d));struct.pack_into('<I',d,12,crc(d));(out/'title_logo.dprm').write_bytes(d)
    mats=b.mat3.materials; tt=128;mt=align(tt+len(textures)*48);st=align(mt+len(mats)*32);pix=align(st+len(mats)*40);total=sum(p[2]*p[3]*2 for p in textures); t=bytearray(pix+total);t[:4]=b'DPTX';struct.pack_into('<HH',t,4,2,128);struct.pack_into('<IIIIII',t,16,len(textures),len(mats),tt,48,mt,32);struct.pack_into('<I',t,48,total);t[64:68]=b'PEV1';struct.pack_into('<IIII',t,68,st,40,len(mats),1);po=pix
    for tid,p in enumerate(textures):
      w,h,sw,sh,fmt,data=p;struct.pack_into('<IHHHHB',t,tt+tid*48,tid,w,h,sw,sh,fmt);struct.pack_into('<II',t,tt+tid*48+16,po,len(data));t[po:po+len(data)]=data;po+=len(data)
    for gid,m in enumerate(mats):
      ids=[x for x in m.textures if x is not None]; primary=source_tex.get(ids[0],white) if ids else white; klass=mat_class(m); struct.pack_into('<HBB',t,mt+gid*32,primary,klass,0); col=m.material_colors[0]; rgba=(col.r,col.g,col.b,col.a) if col else (255,255,255,255);struct.pack_into('<I',t,mt+gid*32+4,c32(rgba));ac=m.alpha_compare;z=m.z_mode;bl=m.blend_mode;off=st+gid*40;struct.pack_into('<H',t,off,gid);t[off+2]=klass;t[off+3]=0 if klass==0 else 1;t[off+4]=int(z.depth_test);t[off+5]=z.depth_func.value;t[off+6]=int(z.depth_write);t[off+7]=m.cull_mode.value;t[off+8]=ac.comp0.value;t[off+9]=ac.ref0;t[off+10]=ac.operation.value;t[off+11]=ac.comp1.value;t[off+12]=ac.ref1;t[off+13]=bl.mode.value;t[off+14]=bl.source_factor.value;t[off+15]=bl.destination_factor.value;t[off+16]=bl.logic_op.value;t[off+17]=1;t[off+18]=0;t[off+19]=1;struct.pack_into('<H',t,off+20,primary)
      for j in range(1,8):struct.pack_into('<H',t,off+20+j*2,0xffff)
    struct.pack_into('<I',t,8,len(t));struct.pack_into('<I',t,12,crc(t));(out/'title_logo.dptx').write_bytes(t)

def hermite(t,p0,v0,m0,p1,v1,m1):
    if p1==p0:return v1
    u=(t-p0)/(p1-p0);u2=u*u;u3=u2*u;dt=p1-p0
    return (2*u3-3*u2+1)*v0+(u3-2*u2+u)*dt*m0+(-2*u3+3*u2)*v1+(u3-u2)*dt*m1
def track(table,desc,t):
    count,index,tangent=desc
    if count==1:return float(table[index])
    stride=3 if tangent==0 else 4; keys=[]
    for i in range(count):
      q=index+i*stride
      if tangent==0: keys.append((float(table[q]),float(table[q+1]),float(table[q+2]),float(table[q+2])))
      else: keys.append((float(table[q]),float(table[q+1]),float(table[q+2]),float(table[q+3])))
    if t<=keys[0][0]:return keys[0][1]
    if t>=keys[-1][0]:return keys[-1][1]
    for a,b in zip(keys,keys[1:]):
      if t<=b[0]:return hermite(t,a[0],a[1],a[3],b[0],b[1],b[2])
    return keys[-1][1]
def export_dpan(bck_path,out):
    b=bck_path.read_bytes(); base=0x20
    if b[base:base+4]!=b'ANK1': raise ValueError('title BCK has no ANK1 chunk')
    duration=struct.unpack_from('>H',b,base+10)[0]; joints=struct.unpack_from('>H',b,base+12)[0]; sc,rc,tc=struct.unpack_from('>HHH',b,base+14); ao,so,ro,to=struct.unpack_from('>IIII',b,base+20); scale=struct.unpack_from('>'+('f'*sc),b,base+so); rot=struct.unpack_from('>'+('h'*rc),b,base+ro); trans=struct.unpack_from('>'+('f'*tc),b,base+to)
    desc=[[struct.unpack_from('>HHH',b,base+ao+j*54+i*6) for i in range(9)] for j in range(joints)]
    if joints!=7: raise ValueError(('title joint contract',joints))
    for ds in desc:
      for axis in range(3):
        d=ds[axis*3+1]
        if d[0]!=1 or rot[d[1]]!=0: raise ValueError('title logo rotation track is not identity')
    samples=duration+1; frame_max=samples; data=bytearray()
    for f in range(samples):
      for j in range(joints):
        ds=desc[j]; s=[track(scale,ds[a*3],f) for a in range(3)]; tr=[track(trans,ds[a*3+2],f) for a in range(3)]
        data+=struct.pack('<10f',*(tr+[0.,0.,0.,1.]+s))
    total=128+48+len(data); p=bytearray(total);p[:4]=b'DPAN';struct.pack_into('<HHI',p,4,1,128,total);struct.pack_into('<III',p,16,1,joints,30);struct.pack_into('<II',p,32,128,48);entry=128;struct.pack_into('<I',p,entry,7);struct.pack_into('<IIII',p,entry+8,duration,samples,joints,0);struct.pack_into('<II',p,entry+24,176,len(data));p[176:]=data;struct.pack_into('<I',p,12,crc(p));(out/'title_logo.dpan').write_bytes(p)

def main():
    if len(sys.argv)!=4: raise SystemExit('usage: title_logo_export.py <titlelogo_tm.bmd> <titlelogo.bck> <outdir>')
    bmd_path=Path(sys.argv[1]); bck_path=Path(sys.argv[2]); out=Path(sys.argv[3]); out.mkdir(parents=True,exist_ok=True)
    export_model(bmd_path,out); export_dpan(bck_path,out)
    for name in ('title_logo.dprm','title_logo.dptx','title_logo.dpan'):
      p=out/name; print(f'{name} bytes={p.stat().st_size} sha256={hashlib.sha256(p.read_bytes()).hexdigest()}')
if __name__=='__main__': main()
