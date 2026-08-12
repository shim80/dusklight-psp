# Visual pipeline recovery baseline

## Result

The previous autonomous fixed point is reopened for source-derived visual
recovery. The tested canonical package is frozen by annotated tag
`psp-pre-visual-pipeline-recovery-v1` on source commit
`1b365e652da8dd6571e93f44a34980add574f58f`.

VISUAL_BUILD_ID:
`sha256:54f7448df61c5f3e1d828a1d561002420552817712ced6c2dc947588cf4e1785`.

## Identity components

- EBOOT SHA-256: `79e86b2ff186d8558c5606e0e3466681ca8ab418254c3e48fd325513fef6e3bb`;
- ELF SHA-256: `1438decd403789d2c4004fb1ed532a35e11aac6f5ea57df6b5d6770dc17e5961`;
- resource manifest SHA-256: `d854ba906d29302ae1881d1673d34f5fdec46562bdf59561145c220623675c78`;
- package set SHA-256: `3a5c6955ff0a0593c0fa325ae019cfd6484ddaa8da0e6cc48d7964d8360105c1`;
- desktop oracle commit: `1bae8a5e6a812217ca33ba533e707ecfa64b1553`;
- desktop/PSP trace schemas: DTRC v3 / v3.1;
- render contract: `DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1`;
- scenario set SHA-256: `fe04f2dbd9992d18cabf9079fb16a083d8ee3ccc2099cd8397b5c56c3fe0da4e`;
- worker SHA-256: `d7807a667fb2a47d7d77530aa34212ccbe08f27033c2e5c160958a8ae48a2a2f`;
- collector SHA-256: `b1be409d5112c3c7b775fda8751b77be623970b0aabd3b9ed7c9441e048930f4`.

The audit corrected one metadata typo in `CAUSAL_RESUME_STATE.json`: the
desktop oracle commit had an extra trailing character. The authoritative local
source lock and local provenance mirror both prove the corrected 40-character
commit above.

## Gates

P4.4, P5.1, P5.4 and P5.5 are reopened as dependency waits. Nineteen V-series
tasks enforce the strict sequence render traces, opaque depth, alpha,
vegetation, UI, lighting, fog/shadows, final captures, performance, single
release and review package.

Release runs remain zero. No network, room, actor, gameplay, combat or audio
expansion occurred.
