#include <aurora/dl.hpp>
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

namespace {

[[noreturn]] void reject_graphics(const char* function) {
    OSPanic(
        __FILE__,
        __LINE__,
        "graphics entry point reached by non-rendering loader probe: %s",
        function);
}

struct ProbeTexObj {
    u32 mode0 = 0;
    u32 mode1 = 0;
    u32 image0 = UINT32_MAX;
    u32 image3 = 0;
    const void* user_data = nullptr;
    const void* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 format = UINT32_MAX;
    GXTlut tlut = GX_TLUT0;
    u32 object_id = 0;
    u32 data_version = 0;
    u8 flags = 0;
};

struct ProbeTlutObj {
    u32 tlut = 0;
    u32 load_tlut0 = 0;
    u16 entries = 0;
    const void* data = nullptr;
    GXTlutFmt format = GX_TL_IA8;
    u32 object_id = 0;
    u32 data_version = 0;
    u8 flags = 0;
};

static_assert(sizeof(ProbeTexObj) <= sizeof(GXTexObj));
static_assert(sizeof(ProbeTlutObj) <= sizeof(GXTlutObj));

constexpr u8 kFilterConversion[6] = {0x00, 0x04, 0x01, 0x05, 0x02, 0x06};
u32 next_texture_id = 1;
u32 next_tlut_id = 1;

void set_field(u32& reg, u32 size, u32 shift, u32 value) {
    const u32 mask = ((1u << size) - 1u) << shift;
    reg = (reg & ~mask) | (value << shift);
}

void init_texture(
    ProbeTexObj& object,
    const void* data,
    u16 width,
    u16 height,
    u32 format,
    GXTexWrapMode wrap_s,
    GXTexWrapMode wrap_t,
    GXBool mipmap) {
    object = {};
    object.width = width;
    object.height = height;
    object.format = format;
    object.tlut = GX_TLUT0;
    object.flags = 2;
    object.object_id = next_texture_id++;
    object.data_version = 1;

    set_field(object.mode0, 2, 0, wrap_s);
    set_field(object.mode0, 2, 2, wrap_t);
    set_field(object.mode0, 1, 4, 1);
    if (mipmap) {
        object.flags |= 1;
        object.mode0 = (object.mode0 & 0xffffff1f) | 0xc0;
        const u32 maximum = std::max<u32>(width, height);
        const u8 lod = static_cast<u8>(
            16.0f * static_cast<float>(31 - __builtin_clz(maximum)));
        set_field(object.mode1, 8, 8, lod);
    } else {
        object.mode0 = (object.mode0 & 0xffffff1f) | 0x80;
    }
    set_field(object.image0, 10, 0, width - 1);
    set_field(object.image0, 10, 10, height - 1);
    set_field(object.image0, 4, 20, format & 0xf);
    object.data = data;
}

}

namespace aurora::gx::dl {

std::optional<std::vector<u8>> optimize(
    const u8*,
    u32,
    const GXVtxDescList*,
    const VtxFmtLists*) {
    // The renderer-free probe retains the original display list byte-for-byte.
    return std::nullopt;
}

}

extern "C" {

void GXCallDisplayList(const void*, u32) {
    reject_graphics(__func__);
}

void GXCmd1u8(u8) {
    reject_graphics(__func__);
}

void GXCmd1u16(u16) {
    reject_graphics(__func__);
}

void GXCmd1u32(u32) {
    reject_graphics(__func__);
}

void GXCmd1u64(u64) {
    reject_graphics(__func__);
}

void GXDestroyTexObj(GXTexObj*) {
    // Object lifetime bookkeeping only; no renderer owns a probe texture.
}

void GXInitTexObj(
    GXTexObj* object,
    const void* data,
    u16 width,
    u16 height,
    GXTexFmt format,
    GXTexWrapMode wrap_s,
    GXTexWrapMode wrap_t,
    GXBool mipmap) {
    init_texture(
        *reinterpret_cast<ProbeTexObj*>(object),
        data,
        width,
        height,
        format,
        wrap_s,
        wrap_t,
        mipmap);
}

void GXInitTexObjCI(
    GXTexObj* object,
    const void* data,
    u16 width,
    u16 height,
    GXCITexFmt format,
    GXTexWrapMode wrap_s,
    GXTexWrapMode wrap_t,
    GXBool mipmap,
    u32 tlut) {
    auto& texture = *reinterpret_cast<ProbeTexObj*>(object);
    init_texture(
        texture,
        data,
        width,
        height,
        format,
        wrap_s,
        wrap_t,
        mipmap);
    texture.tlut = static_cast<GXTlut>(tlut);
    texture.flags &= ~2u;
}

void GXInitTexObjLOD(
    GXTexObj* object,
    GXTexFilter min_filter,
    GXTexFilter mag_filter,
    f32 min_lod,
    f32 max_lod,
    f32 lod_bias,
    GXBool bias_clamp,
    GXBool edge_lod,
    GXAnisotropy anisotropy) {
    auto& texture = *reinterpret_cast<ProbeTexObj*>(object);
    const f32 clamped_bias = std::clamp(lod_bias, -4.0f, 3.99f);
    set_field(texture.mode0, 8, 9, static_cast<u8>(32.0f * clamped_bias));
    set_field(texture.mode0, 1, 4, mag_filter == GX_LINEAR);
    set_field(texture.mode0, 3, 5, kFilterConversion[min_filter]);
    set_field(texture.mode0, 1, 8, edge_lod ? 0 : 1);
    texture.mode0 &= 0xfff9ffff;
    set_field(texture.mode0, 2, 19, anisotropy);
    set_field(texture.mode0, 1, 21, bias_clamp);
    set_field(
        texture.mode1,
        8,
        0,
        static_cast<u8>(16.0f * std::clamp(min_lod, 0.0f, 10.0f)));
    set_field(
        texture.mode1,
        8,
        8,
        static_cast<u8>(16.0f * std::clamp(max_lod, 0.0f, 10.0f)));
}

void GXInitTexObjTlut(GXTexObj* object, u32 tlut) {
    reinterpret_cast<ProbeTexObj*>(object)->tlut = static_cast<GXTlut>(tlut);
}

void GXInitTlutObj(
    GXTlutObj* object,
    const void* data,
    GXTlutFmt format,
    u16 entries) {
    std::memset(object, 0, sizeof(*object));
    auto& tlut = *reinterpret_cast<ProbeTlutObj*>(object);
    tlut.data = data;
    tlut.format = format;
    tlut.entries = entries;
    tlut.object_id = next_tlut_id++;
    tlut.data_version = 1;
    set_field(tlut.tlut, 2, 10, format);
    set_field(tlut.load_tlut0, 8, 24, 0x64);
}

void GXLoadTexMtxImm(const void*, u32, GXTexMtxType) {
    reject_graphics(__func__);
}

void GXLoadTexObj(GXTexObj*, GXTexMapID) {
    reject_graphics(__func__);
}

void GXLoadTlut(const GXTlutObj*, u32) {
    reject_graphics(__func__);
}

void GXSetArray(GXAttr, const void*, u32, u8, bool) {
    reject_graphics(__func__);
}

}
