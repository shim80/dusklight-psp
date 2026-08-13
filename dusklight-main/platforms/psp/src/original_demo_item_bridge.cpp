#include "d/actor/d_a_demo_item.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "dusk/psp/demo_item_runtime.hpp"
#include "dusk/psp/model_runtime.hpp"

#include <cstring>

namespace {

constexpr std::uint32_t kHeartPieceBckResource = 5;
constexpr std::uint32_t kHeartPieceBrkResource = 11;

J3DModelData g_demo_item_model_data;
J3DAnmTransform g_demo_item_bck;
J3DAnmTevRegKey g_demo_item_brk;
bool g_demo_item_animations_ready = false;

bool heart_piece(u8 item) {
    return item == dItemNo_KAKERA_HEART_e;
}

}  // namespace

namespace dusk::psp::demo_item {

bool configure_animation_resources(
    const playable::PackageView& bck_package,
    std::uint32_t bck_resource_id,
    const playable::PackageView& brk_package,
    std::uint32_t brk_resource_id) {
    clear_animation_resources();
    if (bck_resource_id != kHeartPieceBckResource ||
        brk_resource_id != kHeartPieceBrkResource ||
        !g_demo_item_bck.configure(
            bck_package, bck_resource_id) ||
        !g_demo_item_brk.configure(
            brk_package, brk_resource_id)) {
        clear_animation_resources();
        return false;
    }
    g_demo_item_animations_ready = true;
    return true;
}

void clear_animation_resources() {
    g_demo_item_bck = {};
    g_demo_item_brk = {};
    g_demo_item_animations_ready = false;
    g_demo_item_model_data.clear_material_animation();
}

const J3DModelData* source_model_data() {
    return &g_demo_item_model_data;
}

}  // namespace dusk::psp::demo_item

const daItemBase_data daItemBase_c::m_data = {
    -4.5f, 0.62f, 45.0f, 10.0f, 6.0f, 1, 240, 60, 4000, 120,
    -4.0f, 3.5f, 1100, 3000, 23.0f, -6.0f, 13, 10,
};

u8 daItemBase_c::getItemNo() { return m_itemNo; }
void daItemBase_c::hide() { field_0x92b &= static_cast<u8>(~1u); }
void daItemBase_c::show() { field_0x92b |= 1u; }
void daItemBase_c::changeDraw() { chkDraw() ? hide() : show(); }
bool daItemBase_c::chkDraw() { return (field_0x92b & 1u) != 0; }
void daItemBase_c::dead() { field_0x92b |= 2u; }
bool daItemBase_c::chkDead() { return (field_0x92b & 2u) != 0; }

int daItemBase_c::CreateItemHeap(
    const char* archive, s16 bmd, s16 btk, s16 bpk,
    s16 bck, s16 bxa, s16 brk, s16 btp) {
    // The static PSP presentation remains the fallback until authorized
    // source packages are configured. The animation path only accepts the
    // recovered Heart Piece identities and never substitutes guessed curves.
    if (!heart_piece(m_itemNo) || archive == nullptr ||
        std::strcmp(archive, "O_gD_hutk") != 0 || bmd != 8 ||
        btk > 0 || bpk > 0 || bxa > 0 || btp > 0) {
        return 0;
    }

    mpModel = JKR_NEW J3DModel();
    if (mpModel == nullptr) {
        return 0;
    }
    mpModel->duskPspConfigureStub(&g_demo_item_model_data, this);

    mpBckAnm = nullptr;
    mpBrkAnm = nullptr;
    if (g_demo_item_animations_ready) {
        if (bck != static_cast<s16>(kHeartPieceBckResource) ||
            brk != static_cast<s16>(kHeartPieceBrkResource)) {
            return 0;
        }
        mpBckAnm = JKR_NEW mDoExt_bckAnm();
        if (mpBckAnm == nullptr ||
            !mpBckAnm->init(
                &g_demo_item_bck, TRUE,
                static_cast<int>(
                    dusk::psp::animation::LoopMode::Loop),
                1.0f, 0, -1, false)) {
            return 0;
        }

        const int brk_play = getTevFrm() == -1 ? TRUE : FALSE;
        mpBrkAnm = JKR_NEW mDoExt_brkAnm();
        if (mpBrkAnm == nullptr ||
            !mpBrkAnm->init(
                &g_demo_item_model_data, &g_demo_item_brk,
                brk_play,
                static_cast<int>(
                    dusk::psp::animation::LoopMode::Loop),
                1.0f, 0, -1)) {
            return 0;
        }
    }

    if (!clothCreate()) {
        return 0;
    }
    return __CreateHeap() ? TRUE : FALSE;
}

int daItemBase_c::DeleteBase(const char* archive) {
    dComIfG_resDelete(&mPhase, archive);
    return 1;
}

void daItemBase_c::setListEnd() {}

void daItemBase_c::animPlay(
    f32, f32, f32 bck_speed, f32,
    f32 brk_speed, f32) {
    if (mpBrkAnm != nullptr && getTevFrm() == -1) {
        mpBrkAnm->setPlaySpeed(brk_speed);
        mpBrkAnm->play();
    }
    if (mpBckAnm != nullptr) {
        mpBckAnm->setPlaySpeed(bck_speed);
        mpBckAnm->play();
    }
}

int daItemBase_c::DrawBase() {
    if (mpModel == nullptr) {
        return 0;
    }
    setTevStr();
    animEntry();
    setListStart();
    settingBeforeDraw();
    mDoExt_modelUpdateDL(mpModel);
    setListEnd();
    setShadow();
    return 1;
}

void daItemBase_c::setListStart() {}
void daItemBase_c::settingBeforeDraw() {}
void daItemBase_c::setTevStr() {}
void daItemBase_c::setShadow() {}

void daItemBase_c::animEntry() {
    if (mpModel == nullptr) {
        return;
    }
    if (mpBrkAnm != nullptr) {
        const s8 tev_frame = getTevFrm();
        if (tev_frame != -1) {
            mpBrkAnm->entry(
                mpModel->getModelData(),
                static_cast<float>(tev_frame));
        } else {
            mpBrkAnm->entry(mpModel->getModelData());
        }
    }
    if (mpBckAnm != nullptr) {
        mpBckAnm->entry(mpModel->getModelData());
    }
}

void daItemBase_c::RotateYBase() {}
int daItemBase_c::clothCreate() { return 1; }
int daItemBase_c::__CreateHeap() { return 1; }
BOOL daItemBase_c::chkFlag(int flag) { return dItem_data::chkFlag(m_itemNo, flag); }
s8 daItemBase_c::getTevFrm() { return dItem_data::getTevFrm(m_itemNo); }
s8 daItemBase_c::getBtpFrm() { return dItem_data::getBtpFrm(m_itemNo); }
u8 daItemBase_c::getShadowSize() { return dItem_data::getShadowSize(m_itemNo); }
u8 daItemBase_c::getCollisionH() { return dItem_data::getH(m_itemNo); }
u8 daItemBase_c::getCollisionR() { return dItem_data::getR(m_itemNo); }

int CheckItemCreateHeap(fopAc_ac_c* actor) {
    auto* item = static_cast<daItemBase_c*>(actor);
    const u8 no = item->getItemNo();
    return item->CreateItemHeap(
        dItem_data::getArcName(no), dItem_data::getBmdName(no),
        dItem_data::getBtkName(no), dItem_data::getBpkName(no),
        dItem_data::getBckName(no), dItem_data::getBxaName(no),
        dItem_data::getBrkName(no), dItem_data::getBtpName(no));
}

const char* dItem_data::getArcName(u8 item) {
    return heart_piece(item) ? "O_gD_hutk" : nullptr;
}
s16 dItem_data::getBmdName(u8 item) { return heart_piece(item) ? 8 : -1; }
s16 dItem_data::getBtkName(u8) { return -1; }
s16 dItem_data::getBpkName(u8) { return -1; }
s16 dItem_data::getBckName(u8 item) { return heart_piece(item) ? 5 : -1; }
s16 dItem_data::getBxaName(u8) { return -1; }
s16 dItem_data::getBrkName(u8 item) { return heart_piece(item) ? 11 : -1; }
s16 dItem_data::getBtpName(u8) { return -1; }
s8 dItem_data::getTevFrm(u8) { return -1; }
s8 dItem_data::getBtpFrm(u8) { return -1; }
u8 dItem_data::getShadowSize(u8) { return 0; }
u8 dItem_data::getH(u8) { return 0; }
u8 dItem_data::getR(u8) { return 0; }

bool isInsect(u8 item) { return item >= 0xC0 && item <= 0xD7; }
