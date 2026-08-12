# V4 alpha source inventory

## Result

`V4A_ALPHA_SOURCE_INVENTORY=DONE`

The pinned local source image was read through the existing host loader with
network proxies forced to a closed loopback endpoint. No source resource was
written to the repository. A diagnostic-only loader path reports the effective
J3D PE state after loading; it does not change a package format, a PSP bucket,
or the canonical renderer.

The inventory covers the three accepted rooms, the four Link models, the title
model, and the accepted room-stress objects. It contains **120 source
materials** and **107 3D textures**:

| Class | Materials |
|---|---:|
| OPAQUE | 93 |
| ALPHA_TEST | 8 |
| ALPHA_BLEND | 14 |
| ADDITIVE | 0 |
| MULTIPLY | 1 |
| UNSUPPORTED_COMPLEX | 4 |

The decisive finding for V4 is that `material_mode`/draw buffer is not a
sufficient bucket oracle. F_SP108 materials 17–21 and five TitlePal materials
have an OPAQUE draw-buffer tag but carry active source-alpha blending. V4 must
serialize the effective PE state; it must not derive alpha solely from
`J3DMaterial::getMaterialMode()` or texture `alphaEnabled`.

## Exact classification rule used for the audit

The audit decodes the stored IDs directly and therefore does not depend on the
mutable J3D lookup tables:

- Z: `test = mZModeID >= 16`, `func = (mZModeID & 15) / 2`,
  `write = mZModeID & 1`;
- alpha: `comp0 = (mID >> 5) & 7`, `op = (mID >> 3) & 3`,
  `comp1 = mID & 7`, plus both stored references;
- blend and cull: the effective `J3DBlendInfo` fields and color-block cull
  value;
- OPAQUE: no blend and a compare which always passes;
- ALPHA_TEST: no blend and a non-trivial alpha compare;
- ALPHA_BLEND: source-alpha / inverse-source-alpha blending;
- ADDITIVE: one / one blending;
- MULTIPLY: destination-color / zero (or its symmetric form);
- UNSUPPORTED_COMPLEX: every other blend/logic/subtract combination.

GX compare values are retained numerically in the evidence. The relevant
values here are `3=LEQUAL`, `6=GEQUAL`, `7=ALWAYS`; alpha operations are
`0=AND`, `1=OR`. Blend values are recorded as
`mode/src/dst/logic`.

## Inventory by real source model

| Source model | Materials | Provisional classes | Source textures |
|---|---:|---|---:|
| `F_SP108/R01/model.bmd` | 22 | 7 opaque, 5 alpha-test, 6 alpha-blend, 1 multiply, 3 complex | 18 |
| `D_MN10/R09/model.bmd` | 23 | 22 opaque, 1 alpha-blend | 22 |
| `D_MN10/R02/model.bmd` | 19 | 17 opaque, 2 alpha-blend | 17 |
| `al.bmd` | 18 | 18 opaque | 12 |
| `al_head.bmd` | 2 | 2 opaque | 2 |
| `al_hands.bmd` | 11 | 11 opaque | 1 |
| `al_face.bmd` | 5 | 3 opaque, 2 alpha-test | 14 |
| `TitlePal.arc`, model 10 | 6 | 5 alpha-blend, 1 complex | 8 |
| `L4HsMato.arc`, model 4 | 2 | 1 opaque, 1 alpha-test | 2 |
| `P_Gear.arc`, models 4/3 | 4 | 4 opaque | 4 |
| `L4R02Gate.arc`, model 4 | 4 | 4 opaque | 4 |
| `P_Sswitch.arc`, models 4/5 | 2 | 2 opaque | 2 |
| `Dalways.arc`, model 13 | 2 | 2 opaque | 1 |

All material identities, including the opaque members, are:

- F_SP108: `0 aa_IsekiRock_v`, `1 aa_MA04_Kabe_v`,
  `2 aa_MA04_KoyojyuMiki_v`, `3 aa_MA04_KusaA_v`,
  `4 aa_MA04_KusaB_v`, `5 aa_MA04_Mountain_v`,
  `6 aa_MA04_Tuti_v`, `7 bb_KiBack_v`, `8 bb_KiLow_v`,
  `9 bb_KoyojyuLeaf_v`, `10 bb_MA04_Iriguchi_v`,
  `11 bb_MA04_Sida_v`, `12 cc_MA06_water_v_x`,
  `13 cf_MA03_water01_v`, `14 cf_MA03_water02_v`,
  `15 cf_MA03_water02_v_x`, `16 cg_MA09_water_v`,
  `17 ff_MA04_KiwaTuti_v_x`, `18 gg_MA04_KiwaKusaA_v_x`,
  `19 hh_MA04_KiwaKusaB_v_x`, `20 jj_MA04_KiwaKabe_v_x`,
  `21 ll_MA04_Kiwa_Mountain_v_x`.
- D_MN10/R09: `0 aa_GoldLineA_v`, `1 aa_GoldLineB_v`,
  `2 aa_LineB_v`, `3 aa_Lv4_floor_v`, `4 aa_Lv4wallA_v`,
  `5 aa_Lv4wallB_v`, `6 aa_Neji_v`, `7 aa_Spin_rale_v`,
  `8 aa_center_v`, `9 aa_hekigaA_v`, `10 aa_hekigaB_v`,
  `11 aa_kenjya_v`, `12 aa_kuzureA_v`, `13 aa_poleA_v`,
  `14 aa_poleD_v`, `15 aa_sand0_v`, `16 aa_sand1s_v`,
  `17 aa_simasima_v`, `18 aa_stepA_v`, `19 aa_tileA_v`,
  `20 aa_tileB_v`, `21 aa_yari_v`, `22 dd_sand0_v_x`.
- D_MN10/R02: `0 aa_FloorA_v`, `1 aa_GoldLineA_v`,
  `2 aa_GoldLineB_v`, `3 aa_LineB_v`, `4 aa_Lv4wallA_v`,
  `5 aa_Lv4wallA_v_x`, `6 aa_Lv4wallB_v`, `7 aa_center_v`,
  `8 aa_hekigaC_v`, `9 aa_kenjya_v`, `10 aa_mahou_soto_v`,
  `11 aa_poleA_v`, `12 aa_sand1s_v`, `13 aa_sand2_v`,
  `14 aa_stepA_v`, `15 aa_tileA_v`, `16 aa_tileB_v`,
  `17 cc_jyuzuR02_v`, `18 dd_sand0_v_x`.
- Link body: `al_armL_m`, `al_armR_m`, `al_arm_m`, `al_bag_m`,
  `al_beltS_m`, `al_beltW_m`, `al_boots_m`, `al_ear_m`,
  `al_earring_m`, `al_eri_m`, `al_gauntletL_m`, `al_handLA_m`,
  `al_handRA_m`, `al_inner_m`, `al_lowbody_m`, `al_pants_m`,
  `al_skirt_m`, `al_upbody_m`. Head: `al_cap_m`, `al_hair_m`.
  Hands: `al_handLB_m`, `al_handLC_m`, `al_handLE_m`,
  `al_handLF_m`, `al_handLG_m`, `al_handRB_m`, `al_handRC_m`,
  `al_handRD_m`, `al_handRE_m`, `al_handRF_m`, `al_handRG_m`.
  Face: `al_eyeL_m`, `al_eyeR_m`, `al_eyeballL_m`,
  `al_eyeballR_m`, `al_face_m`.
- TitlePal: `mat_msk_wl`, `mat_nintendo`, `mat_tp`,
  `mat_zeldaBlur`, `mat_zleda`, `pasted__mat_zleda`.
- Objects: L4HsMato `aa_Lv4target1_v`, `aa_Lv4target_v`;
  P_Gear 4/3 `lambert100_v`, `lambert101_v`; L4R02Gate
  `aa_GoldLineA_v`, `aa_LineC_v`, `aa_Lv4wallA_v`, `aa_kenjya_v`;
  both P_Sswitch models `aa_Swi_v`; Dalways `BoxB1`, `BoxB1(2)`.

## Non-opaque and complex state records

Columns are `Z=test/func/write`,
`alpha=comp0/ref0/op/comp1/ref1`, `blend=mode/src/dst/logic`, and the source
texture slots as loaded.

| Model | ID | Material | Class | Draw buffer | Z | Alpha | Blend | Cull | Textures |
|---|---:|---|---|---|---|---|---|---:|---|
| `D_MN10_R02/model.bmd` | 5 | `aa_Lv4wallA_v_x` | ALPHA_BLEND | XLU | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `12,0,0,0` |
| `D_MN10_R02/model.bmd` | 18 | `dd_sand0_v_x` | ALPHA_BLEND | XLU | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 0 | `0,0,0,0` |
| `D_MN10_R09/model.bmd` | 22 | `dd_sand0_v_x` | ALPHA_BLEND | XLU | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 0 | `0,0,0,0` |
| `F_SP108_R01/model.bmd` | 7 | `bb_KiBack_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 2 | `15,0,0,0` |
| `F_SP108_R01/model.bmd` | 8 | `bb_KiLow_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 0 | `14,0,0,0` |
| `F_SP108_R01/model.bmd` | 9 | `bb_KoyojyuLeaf_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 0 | `13,0,0,0` |
| `F_SP108_R01/model.bmd` | 10 | `bb_MA04_Iriguchi_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 0 | `12,0,0,0` |
| `F_SP108_R01/model.bmd` | 11 | `bb_MA04_Sida_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 0 | `11,0,0,0` |
| `F_SP108_R01/model.bmd` | 12 | `cc_MA06_water_v_x` | ALPHA_BLEND | XLU | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 0 | `10,0,0,0` |
| `F_SP108_R01/model.bmd` | 13 | `cf_MA03_water01_v` | UNSUPPORTED_COMPLEX | XLU | 1/3/0 | 3/65/1/6/210 | 1/4/1/3 | 0 | `6,6,0` |
| `F_SP108_R01/model.bmd` | 14 | `cf_MA03_water02_v` | UNSUPPORTED_COMPLEX | XLU | 1/3/0 | 3/65/1/6/210 | 1/4/1/3 | 2 | `9,6,0` |
| `F_SP108_R01/model.bmd` | 15 | `cf_MA03_water02_v_x` | UNSUPPORTED_COMPLEX | XLU | 1/3/0 | 7/0/1/7/0 | 1/4/1/3 | 0 | `8,6,0` |
| `F_SP108_R01/model.bmd` | 16 | `cg_MA09_water_v` | MULTIPLY | XLU | 1/3/0 | 7/0/1/7/0 | 1/2/0/3 | 0 | `6,7,6,0` |
| `F_SP108_R01/model.bmd` | 17 | `ff_MA04_KiwaTuti_v_x` | ALPHA_BLEND | OPAQUE | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `5,1,0` |
| `F_SP108_R01/model.bmd` | 18 | `gg_MA04_KiwaKusaA_v_x` | ALPHA_BLEND | OPAQUE | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `4,1,0` |
| `F_SP108_R01/model.bmd` | 19 | `hh_MA04_KiwaKusaB_v_x` | ALPHA_BLEND | OPAQUE | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `3,1,0` |
| `F_SP108_R01/model.bmd` | 20 | `jj_MA04_KiwaKabe_v_x` | ALPHA_BLEND | OPAQUE | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `2,1,0` |
| `F_SP108_R01/model.bmd` | 21 | `ll_MA04_Kiwa_Mountain_v_x` | ALPHA_BLEND | OPAQUE | 1/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `0,1,0` |
| `al_face.bmd` | 0 | `al_eyeL_m` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 2 | `13,0,0,0` |
| `al_face.bmd` | 1 | `al_eyeR_m` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 2 | `13,0,0,0` |
| `L4HsMato:4` | 0 | `aa_Lv4target1_v` | ALPHA_TEST | OPAQUE | 1/3/1 | 6/128/0/3/255 | 0/1/0/3 | 2 | `1,0,0,0` |
| `TitlePal:10` | 0 | `mat_msk_wl` | ALPHA_BLEND | OPAQUE | 0/3/1 | 7/0/1/7/0 | 1/4/5/3 | 2 | `6,0` |
| `TitlePal:10` | 1 | `mat_nintendo` | ALPHA_BLEND | OPAQUE | 0/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `7,0` |
| `TitlePal:10` | 2 | `mat_tp` | ALPHA_BLEND | OPAQUE | 0/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `5,0` |
| `TitlePal:10` | 3 | `mat_zeldaBlur` | UNSUPPORTED_COMPLEX | OPAQUE | 0/3/0 | 7/0/1/7/0 | 1/4/1/3 | 2 | `0,0,1,0` |
| `TitlePal:10` | 4 | `mat_zleda` | ALPHA_BLEND | OPAQUE | 0/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `1,1,2,3,0` |
| `TitlePal:10` | 5 | `pasted__mat_zleda` | ALPHA_BLEND | OPAQUE | 0/3/0 | 7/0/1/7/0 | 1/4/5/3 | 2 | `4,0` |

The remaining 93 records all share the effective opaque signature:
Z `1/3/1`, alpha `7/0/1/7/0`, blend `0/1/0/3`, cull `2`.

## Vegetation inventory

The F_SP108 vegetation seam is source-identifiable and is not a generic
texture-alpha guess:

| Source material | Texture 0 | State | Cull |
|---|---|---|---:|
| `bb_KiBack_v` | `C_a_HM_Kikabe` (CMPR) | alpha-test GEQUAL 128 AND LEQUAL 255 | 2 |
| `bb_KiLow_v` | `C_a_HM_Kilow` (CMPR) | same alpha-test | 0 |
| `bb_KoyojyuLeaf_v` | `C_a_HM_Leaf000` (CMPR) | same alpha-test | 0 |
| `bb_MA04_Sida_v` | `C_a_HM_Sida` (CMPR) | same alpha-test | 0 |
| `gg_MA04_KiwaKusaA_v_x` | `C_a_HM_Kusa00` + shade texture | standard alpha blend, no Z write | 2 |
| `hh_MA04_KiwaKusaB_v_x` | `C_a_HM_Kusa01` + shade texture | standard alpha blend, no Z write | 2 |

`bb_MA04_Iriguchi_v` uses the same cutout contract but is classified as an
entrance surface rather than vegetation. The corresponding opaque vegetation
interiors are `aa_MA04_KusaA_v`, `aa_MA04_KusaB_v`, and
`aa_MA04_KoyojyuMiki_v`.

## Texture and palette audit

The actual 3D inventory is:

| GX format | Count | Alpha relevance |
|---|---:|---|
| CMPR (`0x0e`) | 87 | may encode one-bit transparency; source PE state remains authoritative |
| I4 (`0x00`) | 4 | intensity is replicated to alpha by the current decoder |
| I8 (`0x01`) | 4 | intensity is replicated to alpha by the current decoder |
| IA4 (`0x02`) | 3 | explicit 4-bit intensity and 4-bit alpha |
| RGB5A3 (`0x05`) | 6 | 3-bit alpha or opaque RGB555 |
| RGBA8 (`0x06`) | 3 | explicit 8-bit alpha in planar AR/GB blocks |
| IA8 (`0x03`) | 0 | supported and tested, absent from these 3D models |
| C4/C8 | 0 | absent from these 3D models |

TitlePal is the RGBA8-heavy case (`Zelda2_TM`, `TwilightPrincess`,
`MaskWolf`) and also has IA4 `Nintendo`. Link's face owns the six RGB5A3
textures. F_SP108 owns two IA4 water entries and three I4 effect maps.

The accepted HUD inventory adds one real indexed case:
`tt_rupy_green_icon2.bti` is C8 with a 208-entry RGB5A3 TLUT. No C4 or IA8
TLUT source was found in the accepted set. An earlier HUD report labels numeric
format `2` as IA8; the source `GXTexFmt` enum and the active decoder both define
`2` as **IA4** and `3` as IA8. V4 uses the enum definition and treats the old
label as stale documentation, not as source evidence.

## Host tests and readiness

The new independent reference test exercises alpha placement and edge cases
for IA4, IA8, RGB5A3, RGBA8, C4, C8, CMPR, TLUT RGB5A3, and TLUT IA8. It
passes with:

```text
ALPHA_TEXTURE_DECODER_REFERENCE_OK formats=IA4,IA8,RGB5A3,RGBA8,C4,C8,CMPR tlut=RGB5A3,IA8
```

The instrumented loader was compiled once and successfully exercised against
F_SP108/R01, D_MN10/R09, D_MN10/R02, all four Link models, all accepted
room-stress object models, and TitlePal. The room runs used the canonical
128-pixel conversion bound but the inventory itself reports unmodified source
metadata.

`V4B_ALPHA_TEXTURE_DECODER_TESTS=DONE`

Runtime integration remains gated by V3D. When that gate opens, the minimal
honest V4 data seam is a generic material record containing draw buffer, exact
Z state, exact alpha compare/references, exact blend state, cull, and all used
texture identities. No rule specific to F_SP108, Link, water, or vegetation is
required.
