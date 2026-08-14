#!/usr/bin/env python3
from __future__ import annotations
import dataclasses, hashlib, math, os, struct, sys, zlib
from pathlib import Path
from collections import OrderedDict

if not hasattr(dataclasses, '_recursive_repr') and hasattr(dataclasses, 'recursive_repr'):
    dataclasses._recursive_repr = dataclasses.recursive_repr

GCLIB = Path(os.environ.get('GCLIB_ROOT', ''))
if not GCLIB.is_dir():
    raise SystemExit('GCLIB_ROOT must point to a pinned gclib source checkout')
sys.path.insert(0, str(GCLIB))

# gclib 6412774 predates Python 3.13's stricter GenericAlias handling.
# Patch only the two read helpers in memory; never modify the pinned checkout.
import types
import typing
import gclib.bunfoe as _bunfoe
from gclib import fs_helpers as _fs
from gclib.fs_helpers import FixedStr, MagicStr
_original_get_byte_size = _bunfoe.BUNFOE.get_byte_size
_original_read_value = _bunfoe.BUNFOE.read_value

def _compat_get_byte_size(field_type):
    if isinstance(field_type, types.GenericAlias) and field_type.__origin__ in (FixedStr, MagicStr):
        return typing.get_args(field_type)[0]
    return _original_get_byte_size(field_type)

def _compat_read_value(self, field_type, offset):
    if isinstance(field_type, types.GenericAlias) and field_type.__origin__ in (FixedStr, MagicStr):
        return _fs.read_str(self.data, offset, typing.get_args(field_type)[0])
    return _original_read_value(self, field_type, offset)

_bunfoe.BUNFOE.get_byte_size = staticmethod(_compat_get_byte_size)
_bunfoe.BUNFOE.read_value = _compat_read_value

# texture_utils imports imagequant even for decode-only use. Provide a minimal
# non-quantizing module when the optional dependency is absent.
try:
    import imagequant  # noqa: F401
except ImportError:
    import types as _types
    _imagequant = _types.ModuleType('imagequant')
    def _no_quantize(*args, **kwargs):
        raise RuntimeError('imagequant is unavailable in decode-only F_SP102 export')
    _imagequant.quantize_pil_image = _no_quantize
    sys.modules['imagequant'] = _imagequant

from gclib.j3d import BMD
from gclib.j3d_chunks.inf1 import INF1NodeType
from gclib import gx_enums as GX
from PIL import Image

MODEL_PATHS=[
 ('R00/model.bmd','R00/bmdr/model.bmd'),('R00/model1.bmd','R00/bmdr/model1.bmd'),
 ('R00/model2.bmd','R00/bmdr/model2.bmd'),('R00/model3.bmd','R00/bmdr/model3.bmd'),
 ('R00/model4.bmd','R00/bmdr/model4.bmd'),('STG/vrbox_kasumim.bmd','STG/bmdp/vrbox_kasumim.bmd'),
 ('STG/vrbox_kumo.bmd','STG/bmdp/vrbox_kumo.bmd'),('STG/vrbox_sora.bmd','STG/bmdp/vrbox_sora.bmd'),
 ('STG/vrbox_sun.bmd','STG/bmdp/vrbox_sun.bmd')]

def align(v,n=16):return (v+n-1)&~(n-1)
def crc(blob):
    b=bytearray(blob);b[12:16]=b'\0'*4;return zlib.crc32(b)&0xffffffff

def swizzle_16(raw:bytes,w:int,h:int)->bytes:
    row=w*2
    if row%16 or h%8: raise ValueError(('bad swizzle dims',w,h))
    out=bytearray()
    for by in range(0,h,8):
      for bx in range(0,row,16):
       for y in range(8): out+=raw[(by+y)*row+bx:(by+y)*row+bx+16]
    return bytes(out)

def pack_image(img:Image.Image,max_dim=128):
    img=img.convert('RGBA')
    if img.width>max_dim or img.height>max_dim:
      scale=min(max_dim/img.width,max_dim/img.height)
      img=img.resize((max(1,round(img.width*scale)),max(1,round(img.height*scale))),Image.Resampling.LANCZOS)
    w,h=img.size; sw=align(w,8);sh=align(h,8)
    pix=list(img.get_flattened_data()) if hasattr(img, 'get_flattened_data') else list(img.getdata())
    alphas={a for _,_,_,a in pix}
    fmt=0 if alphas=={255} else 1 if alphas.issubset({0,255}) else 2
    raw=bytearray(sw*sh*2)
    for y in range(h):
      for x in range(w):
       r,g,b,a=img.getpixel((x,y))
       if fmt==0:v=(r>>3)|((g>>2)<<5)|((b>>3)<<11)
       elif fmt==1:v=(r>>3)|((g>>3)<<5)|((b>>3)<<10)|((1 if a>=128 else 0)<<15)
       else:v=(r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12)
       struct.pack_into('<H',raw,(y*sw+x)*2,v)
    return w,h,sw,sh,fmt,swizzle_16(bytes(raw),sw,sh),bytes(img.tobytes())

def mat_class(m):
    ac=m.alpha_compare; bl=m.blend_mode
    alpha_nontrivial=not(ac.comp0==GX.CompareType.Always and ac.comp1==GX.CompareType.Always)
    if m.pixel_engine_mode==GX.PixelEngineMode.Alpha_Test or alpha_nontrivial:return 1
    if m.pixel_engine_mode==GX.PixelEngineMode.Opaque and bl.mode==GX.BlendMode.None_:return 0
    if bl.mode==GX.BlendMode.Blend:
      if bl.source_factor==GX.BlendFactor.Source_Alpha and bl.destination_factor==GX.BlendFactor.One:return 3
      if bl.source_factor in (GX.BlendFactor.Destination_Color,) if hasattr(GX.BlendFactor,'Destination_Color') else False:return 4
      return 2
    if bl.mode==GX.BlendMode.Subtract:return 3
    return 5

def parse_desc(shp,shape):
    buf=shp.data.getbuffer();off=shp.attribute_table_offset+shape.first_attribute_offset;out=[]
    while True:
      a,t=struct.unpack_from('>II',buf,off);off+=8
      if a==0xff:break
      if t!=3:raise ValueError(('unsupported index type',a,t))
      out.append(a)
    return out

def primitives(shp,shape):
    buf=shp.data.getbuffer();attrs=parse_desc(shp,shape)
    for gi in range(shape.matrix_group_count):
      go=shp.mtx_group_table_offset+(shape.first_matrix_group_index+gi)*8
      size,po=struct.unpack_from('>II',buf,go);q=shp.primitive_data_offset+po;end=q+size
      while q<end:
        cmd=buf[q];q+=1
        if cmd==0:break
        if cmd!=0x98:raise ValueError(('unsupported primitive',hex(cmd)))
        count=struct.unpack_from('>H',buf,q)[0];q+=2
        strip=[]
        for _ in range(count):
          d={}
          for a in attrs:d[a]=struct.unpack_from('>H',buf,q)[0];q+=2
          strip.append(d)
        yield strip

def color32(rgba):
    r,g,b,a=rgba;return r|(g<<8)|(b<<16)|(a<<24)

def main(root:Path,outdir:Path):
    outdir.mkdir(parents=True,exist_ok=True)
    models=[]
    for label,rel in MODEL_PATHS:
      b=BMD(str(root/rel))
      for j in b.jnt1.joints:
       vals=(j.scale.x,j.scale.y,j.scale.z,j.rotation.x,j.rotation.y,j.rotation.z,j.translation.x,j.translation.y,j.translation.z)
       if any(abs(vals[k]-(1 if k<3 else 0))>1e-5 for k in range(9)):raise ValueError((label,'nonidentity joint'))
      mapping={};cur=None
      for n in b.inf1.flat_hierarchy:
       if n.type==INF1NodeType.MATERIAL:cur=n.index
       elif n.type==INF1NodeType.SHAPE:mapping[n.index]=cur
      if len(mapping)!=b.shp1.shape_count:raise ValueError((label,'shape mapping'))
      models.append((label,b,mapping))

    # Global material IDs are deterministic model order + source material order.
    mat_base={};materials=[]
    for label,b,_ in models:
      mat_base[label]=len(materials)
      for i,m in enumerate(b.mat3.materials):materials.append((label,b,i,m))

    # Decode/dedup all material-referenced textures after PSP downscale.
    textures=[];texkey_to_id={}; source_tex_map={}; white_id=None
    def add_img(img):
      packed=pack_image(img); key=(packed[0],packed[1],packed[4],hashlib.sha256(packed[5]).digest())
      if key not in texkey_to_id:
       texkey_to_id[key]=len(textures);textures.append(packed)
      return texkey_to_id[key]
    white_id=add_img(Image.new('RGBA',(1,1),(255,255,255,255)))
    for label,b,_,m in materials:
      ti=next((x for x in m.textures if x is not None),None)
      if ti is not None:
       key=(label,ti)
       if key not in source_tex_map:source_tex_map[key]=add_img(b.tex1.textures[ti].render())

    vertices=[];indices=[];submeshes=[]; vmap={}; bounds=[float('inf')]*3+[-float('inf')]*3
    shape_global=0
    for label,b,mapping in models:
      attrs=b.vtx1.attributes
      for si,shape in enumerate(b.shp1.shapes):
        local_mat=mapping[si];m=b.mat3.materials[local_mat];gid=mat_base[label]+local_mat
        texsrc=next((x for x in m.textures if x is not None),None); texid=source_tex_map.get((label,texsrc),white_id)
        klass=mat_class(m);bucket=0 if klass==0 else 1 if klass==1 else 2
        first=len(indices)
        for strip in primitives(b.shp1,shape):
          strip_ids=[]
          for d in strip:
            pi=d[GX.Attr.Position.value]; ci=d.get(GX.Attr.Color0.value);ti=d.get(GX.Attr.Tex0.value)
            p=attrs[GX.Attr.Position][pi]
            if ci is not None:
             cf=attrs[GX.Attr.Color0][ci]; rgba=tuple(max(0,min(255,round(v*255))) for v in cf)
            else:
             c=m.material_colors[0];rgba=(c.r,c.g,c.b,c.a) if c else (255,255,255,255)
            uv=attrs[GX.Attr.Tex0][ti] if ti is not None else (0.0,0.0)
            key=(label,pi,ci if ci is not None else -1,ti if ti is not None else -1)
            if key not in vmap:
              vmap[key]=len(vertices); vertices.append((float(uv[0]),float(uv[1]),color32(rgba),float(p[0]),float(p[1]),float(p[2])))
              for a in range(3):bounds[a]=min(bounds[a],p[a]);bounds[a+3]=max(bounds[a+3],p[a])
            strip_ids.append(vmap[key])
          for i in range(2,len(strip_ids)):
            tri=(strip_ids[i-2],strip_ids[i-1],strip_ids[i]) if i%2==0 else (strip_ids[i-1],strip_ids[i-2],strip_ids[i])
            if len(set(tri))==3:indices.extend(tri)
        count=len(indices)-first
        flags=(1 if m.z_mode.depth_test else 0)|(2 if m.z_mode.depth_write else 0)
        submeshes.append((first,count,gid,texid,bucket,shape_global,gid,flags,m.z_mode.depth_func.value))
        shape_global+=1

    if len(vertices)>45000 or len(indices)//3>30000 or len(submeshes)>96 or len(materials)>96 or len(textures)>96:raise ValueError('runtime cap')

    # DPRM v1.
    header=bytearray(256);header[:4]=b'DPRM';struct.pack_into('<HH',header,4,1,256)
    struct.pack_into('<IIIIIII',header,16,4,len(vertices),len(indices),len(indices)//3,len(submeshes),len(materials),len(textures))
    struct.pack_into('<6f',header,48,*bounds);section_table=256;struct.pack_into('<I',header,72,section_table)
    struct.pack_into('<IIIIIII',header,80,24,0,4,8,12,16,20)
    sections=bytearray(128);cursor=align(256+128,16);vo=cursor;cursor+=len(vertices)*24;cursor=align(cursor,16);io=cursor;cursor+=len(indices)*2;cursor=align(cursor,16);so=cursor;cursor+=len(submeshes)*48
    for idx,(typ,off,count,stride) in enumerate([(1,vo,len(vertices),24),(2,io,len(indices),2),(3,so,len(submeshes),48),(4,0,len(materials),0)]):
      struct.pack_into('<IIIII',sections,idx*32,typ,off,0,count,stride)
    dprm=bytearray(cursor);dprm[:256]=header;dprm[256:384]=sections
    q=vo
    for u,v,c,x,y,z in vertices:struct.pack_into('<ffIfff',dprm,q,u,v,c,x,y,z);q+=24
    q=io
    for ix in indices:struct.pack_into('<H',dprm,q,ix);q+=2
    q=so
    for first,count,mat,tex,buck,shape,src,flags,df in submeshes:
      struct.pack_into('<IIHHBBHHBB',dprm,q,first,count,mat,tex,buck,0,shape,src,flags,df);q+=48
    struct.pack_into('<I',dprm,8,len(dprm));struct.pack_into('<I',dprm,12,crc(dprm))

    # DPTX v2 + PEV1 source material state.
    th=128;tt=th;mt=align(tt+len(textures)*48,16);st=align(mt+len(materials)*32,16);pix=align(st+len(materials)*40,16)
    total_tex=sum(t[3]*t[2]*2 for t in textures);dptx=bytearray(pix+total_tex);dptx[:4]=b'DPTX';struct.pack_into('<HH',dptx,4,2,128)
    struct.pack_into('<IIIIII',dptx,16,len(textures),len(materials),tt,48,mt,32);struct.pack_into('<I',dptx,48,total_tex);dptx[64:68]=b'PEV1';struct.pack_into('<IIII',dptx,68,st,40,len(materials),1)
    po=pix
    for tid,t in enumerate(textures):
      w,h,sw,sh,fmt,data,_=t;struct.pack_into('<IHHHHB',dptx,tt+tid*48,tid,w,h,sw,sh,fmt);struct.pack_into('<II',dptx,tt+tid*48+16,po,len(data));dptx[po:po+len(data)]=data;po+=len(data)
    for gid,(label,b,mi,m) in enumerate(materials):
      source_ids=[x for x in m.textures if x is not None]
      primary=source_tex_map[(label,source_ids[0])] if source_ids else white_id
      tids=[primary] + [0xffff] * (min(len(source_ids),8)-1) if source_ids else []
      klass=mat_class(m);bucket=0 if klass==0 else 1 if klass==1 else 2
      struct.pack_into('<HBB',dptx,mt+gid*32,primary,bucket,0)
      c=m.material_colors[0];rgba=(c.r,c.g,c.b,c.a) if c else (255,255,255,255);struct.pack_into('<I',dptx,mt+gid*32+4,color32(rgba))
      ac=m.alpha_compare;z=m.z_mode;bl=m.blend_mode;off=st+gid*40
      struct.pack_into('<H',dptx,off,gid);dptx[off+2]=klass;dptx[off+3]=0 if bucket==0 else 1;dptx[off+4]=int(z.depth_test);dptx[off+5]=z.depth_func.value;dptx[off+6]=int(z.depth_write);dptx[off+7]=m.cull_mode.value;dptx[off+8]=ac.comp0.value;dptx[off+9]=ac.ref0;dptx[off+10]=ac.operation.value;dptx[off+11]=ac.comp1.value;dptx[off+12]=ac.ref1;dptx[off+13]=bl.mode.value;dptx[off+14]=bl.source_factor.value;dptx[off+15]=bl.destination_factor.value;dptx[off+16]=bl.logic_op.value;dptx[off+17]=len(tids);dptx[off+18]=0;dptx[off+19]=1
      for j in range(8):struct.pack_into('<H',dptx,off+20+j*2,tids[j] if j<len(tids) else 0xffff)
    struct.pack_into('<I',dptx,8,len(dptx));struct.pack_into('<I',dptx,12,crc(dptx))

    (outdir/'fsp102_environment.dprm').write_bytes(dprm);(outdir/'fsp102_environment.dptx').write_bytes(dptx)
    manifest=f'vertices={len(vertices)}\ntriangles={len(indices)//3}\nsubmeshes={len(submeshes)}\nmaterials={len(materials)}\ntextures={len(textures)}\ntexture_bytes={total_tex}\ndprm_bytes={len(dprm)}\ndptx_bytes={len(dptx)}\n'
    (outdir/'FSP102_ENVIRONMENT.MANIFEST').write_text(manifest)
    print(manifest,end='');print('dprm_sha256='+hashlib.sha256(dprm).hexdigest());print('dptx_sha256='+hashlib.sha256(dptx).hexdigest())
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
