# Causal parity changelog

## Iteration 001 — Link procedure provenance

- scenario set: ten Link scenarios;
- initial divergence: tick 0, `actor_transform.procedure`, desktop 3, PSP 0;
- layer: `TRACE`;
- desktop symbol: `daAlink_c::mProcID` / `PROC_WAIT`;
- PSP symbol previously mislabeled: `RealRoomState::motion_phase` / `MotionPhase::Idle`;
- correction: DTRC v3.1 reports the source procedure as unavailable and keeps motion phase under its own field;
- hardcoded value: false;
- result: false numeric comparison removed; next cause is `MISSING_SOURCE_STATE` in `STATE_INITIALIZATION`;
- tests: DTRC v3/v3.1 host compatibility, 13-checkpoint runtime harness, seven procedure negatives, Allegrex build.

## Iteration 002 — Link procedure runtime state

- scenario reacquired: `link_idle_full_cycle`;
- PSP build: `81050a849db8a2f80018d86630f58f190397887d`;
- parity build ID: `sha256:f8f14b9a818327139f061bc0a49eadd9463f08089ac996afe26d6a8e67ce356a`;
- correction layer: `STATE_INITIALIZATION`;
- correction: source-compatible runtime state with `PROC_MAX` during construction and `PROC_WAIT` after create; emitter reads that state only;
- hardcoded trace value: false;
- procedure result: `MATCH` on the fresh native PSP trace;
- next causal divergence: tick 0, `actor_state.animation_id`, desktop 618, PSP 0;
- global Link result: still divergent; no final parity marker emitted;
- validation: procedure host harness, seven negative fixtures, DTRC v3/v3.1 tests, causal-engine tests, Allegrex build, historical core smoke through the GUI broker, and native idle reacquisition.

## Iteration 003 — Active animation resource identity

- cause: the PSP trace exposed the local `Locomotion` index while the runtime actually sampled source BCK resources retained in DPAN;
- correction: read the active resource ID directly from the validated DPAN clip table (`0x26A`, `0x277`, `0x0CD`), outside the trace emitter;
- host proof: all three locomotion states resolve to their source resource IDs;
- PSP proof: idle `actor_state`, `animation_change`, and `animation_frame` now match resource ID 618 at tick 0;
- ten-scenario reacquisition: 10/10 native traces completed, same EBOOT and parity identity, no broker restart;
- next common causal divergence: Link leg joint coordinates at tick 0, classified `GROUNDING`;
- global result: still divergent; no final marker emitted.

## Iteration 004 — Rejected incomplete foot-callback model

- hypothesis: applying the fixed segment lengths from `daAlink_c::setFootMatrix()` without its angle state would reproduce the desktop leg matrices;
- PSP observation: rejected; knee and ankle errors increased because the source callback also consumes `mFootData1` angles;
- action: the experimental runtime change was removed in a separate corrective commit; no history rewrite was used;
- restored build: `f5d107ad82d099186ae077d0e9f70b004dda9281`;
- restored parity build ID: `sha256:4813a1ea7058982de9761049e129800b51bc69cebba466048df21e8b397b4c40`;
- restored causal divergence: tick 0, left-knee Y, desktop 499.265, PSP 496.288;
- historical core smoke and native idle trace both pass through the GUI broker;
- no parity marker emitted.

## Iteration 005 — Source foot callback after grounding

- cause: the fixed Link leg-chain callback was executed before the PSP grounding pass, while the desktop order applies `setFootMatrix()` after contact resolution;
- correction: rebuild the two source leg chains after grounding and refresh the skin matrices;
- commit: `b5382a7dc383ab21a7fb8e12c0c32523b1106fa2`;
- parity build ID: `sha256:df800f3238a8775bc1f5ec31a49e537ced1c5fa08d200eccfe398740f40d63c9`;
- result: `GROUNDING` closes; the first divergence moves to `CAMERA`, tick 0;
- validation: grounding host references, Allegrex build, historical core smoke, native idle trace and causal engine.

## Iteration 006 — Source chase-camera startup checkpoint

- cause: the PSP camera used adapted distance/height constants and emitted its post-update state where the desktop Link checkpoint observes the prior camera state;
- correction: model style 45 startup from the pinned local `camstyle.dat`, including distance 300, source latitude quantization, settle order and checkpoint lag;
- commit: `b572ab14581375753b3e252c28ce71c088c8dbd3`;
- parity build ID: `sha256:fcd3d249368bf227c02d072d3e053a57746f9f6f9e52f0fa5e1121666a7ccdf4`;
- result: first camera divergence moves from tick 0 to tick 7;
- validation: camera checkpoint host assertions, locomotion/procedure tests, Allegrex build, historical core smoke and native idle trace.

## Iteration 007 — Preserve chase-camera direction

- cause: the reduced PSP camera held radius at 300 and capped its latitude cushion, whereas the source preserves the prior eye-to-center direction during the chase transient;
- correction: derive radius from the prior eye after center motion and retain the source transient cushion envelope;
- commit: `20d8fde`;
- parity build ID: `sha256:b2c257c6999489cbe0b3ad0b332ddbbf2f219c19316ed806dd513fa057dc7780`;
- result: `CAMERA` closes; the first divergence becomes a missing neutral `input_change` at tick 28;
- validation: host locomotion, Allegrex build, historical core smoke, native idle trace and causal engine.

## Iteration 008 — Link demo input release

- cause: the desktop startup demo owns neutral input through tick 27, then releases it at tick 28; the PSP runtime did not represent this source state transition;
- correction: retain a runtime stick angle and reproduce the observed demo release before deriving `mMoveAngle` semantics; the emitter reads the runtime field rather than manufacturing the event;
- commit: `a62238d`;
- parity build ID: `sha256:55d5bad38fc3c24e714ce9c057fec3fa76e2891213fa647ebeb6cc797ed2db8e`;
- result: `INPUT` closes; the first divergence moves to `ANIMATION_RUNTIME`, tick 43, at the controller frame loop;
- validation: release-checkpoint host assertion, locomotion/procedure tests, Allegrex build, historical core smoke, native idle trace and causal engine.

## Iteration 009 — Source animation-controller checkpoint

- cause: the desktop Link path advances its J3D frame controller once during actor creation and again in `execute()` before emitting `animation_frame`, while the PSP pose sampler starts directly at the first runtime update;
- correction: expose the source-equivalent controller checkpoint from the runtime, with the loop period read from the active DPAN clip table;
- hardcoded tick or desktop value: false; neither tick 43 nor frame 0 appears in the correction;
- commit: `0f99b6c663faed19ef8bb5c907166cdd6c6438a5`;
- parity build ID: `sha256:abfa9d4991c0aea63d44f0f3e6b215fa5e98a0c3442fcf3aecc82cfd0659d05d`;
- revealing scenario result: `ANIMATION_RUNTIME` closes; the first idle divergence moves to `STATE_INITIALIZATION`, tick 300, where the desktop has an `actor_state` event and the PSP trace has ended;
- ten-scenario reacquisition: 10/10 native traces completed through the isolated GUI broker; no first divergence remains in `ANIMATION_RUNTIME`;
- next global frontier: `ACTOR_TRANSFORM` at tick 30 for `link_turn_180`; the other nine scenarios first diverge in `STATE_INITIALIZATION`;
- validation: playable-runtime host test, DTRC v3/v3.1 host contract, Allegrex build, one revealing native trace, ten-scenario Link reacquisition and ten causal summaries.

## Iteration 010 — Source wait-turn update order

- cause: the reduced PSP locomotion path rotated Link on the same update that selected `PROC_WAIT_TURN`, using the ordinary 4500-unit movement turn;
- source semantics: `procWaitTurnInit()` stores `mMoveAngle` and preserves the actor yaw, then `procWaitTurn()` applies signed-16-bit `cLib_addCalcAngleS(..., 30, 0x3CDF, 8000)` on later updates;
- correction: retain the wait-turn target separately, preserve yaw on entry, and use the source signed-angle progression while the procedure is active;
- commit: `74a301ad9ccd061219141084b8130b102a15fc75`;
- parity build ID: `sha256:b239555ec5903554998f8ecac93f714b4f97204578f4b1edfe0c0e01c1055b86`;
- result: actor yaw, current angle and shape angle now match at tick 30; causal divergences fall from 7710 to 7336;
- next divergence: `turn_start.yaw_error` at tick 30, classified `ACTOR_TRANSFORM`; desktop emits signed-16 angle units while the PSP trace still emits radians;
- validation: source wait-turn host sequence, real-room locomotion test, Allegrex build, DTRC host contract, canonical build and one native `link_turn_180` acquisition through the isolated GUI broker.

## Iteration 011 — Signed turn-event trace units

- cause: PSP `turn_start` and `turn_end` encoded `yaw_error` in radians while the desktop field is the signed-16 result of `mMoveAngle-current.angle.y`;
- correction: derive the signed-16 source difference and use the source `0x800` event threshold for both transition detection and payload emission;
- commit: `1721a67d9de246306e51646ea9a38a79ef94106f`;
- parity build ID: `sha256:0bb5cb64d453ade0b71104a303483be2350af07a4534bc4d7d4583d59b6e1daa`;
- result: the turn event matches and the first divergence moves from `ACTOR_TRANSFORM` to `ANIMATION_RUNTIME`, still at tick 30;
- next field: `actor_state.animation_id`, desktop resource 563 (`ANM_STEP_TURN`), PSP resource 618 (idle);
- validation: Link locomotion host test, DTRC host contract, Allegrex build, canonical build, one native `link_turn_180` acquisition and causal engine.

## Iteration 012 — Real source wait-turn clip

- cause: the Link DPAN contained only idle, walk and run, so the PSP runtime could not select desktop resource 563 (`ANM_STEP_TURN` / `stepl.bck`);
- correction: export the real archive resource `0x233`, retain its identity in the DPAN contract, and select it from the existing `TurnInPlace` runtime phase at source playback speed `0.7`;
- commits: `7629fbee8b0692951c81f1874efd022c3f2b21ed` and `a4e18fd248aab284766bca89a034dc7841c39dab`;
- parity build ID: `sha256:8acc67ba3b52df5026c129d797cb63cd983b2a71b89fbae657a8cc9a7dd7494e`;
- result: resource 563 and `animation_change` align; divergences fall from 7335 to 7289 and the next field is the entry frame (`0` versus `1.7`);
- validation: deterministic legal-local conversion, four-clip identity test, playable runtime, root anchor, Link locomotion, Allegrex build and one native trace.

## Iteration 013 — Wait-turn controller entry order

- cause: `procWaitTurnInit()` selects `ANM_STEP_TURN` after the ordinary J3D controller advance for that execute pass, while the PSP advanced the newly selected clip immediately;
- correction: expose frame 0 on selection and begin the `0.7` advance on the following update, only for `TurnInPlace`;
- hardcoded tick or desktop value: false;
- commit: `5fffb95212c5198c0e49251526d0471aef87b237`;
- parity build ID: `sha256:32aa35132e31db2d404eab51af9828ea43ad05968598c9ced8ba37a618597c15`;
- result: `ANIMATION_RUNTIME` closes for `link_turn_180`; divergences fall from 7289 to 7280 and the first divergence moves to `GROUNDING`, tick 31;
- ten-scenario reacquisition: 10/10 native traces and causal summaries are valid; 0/10 first divergences are `ANIMATION_RUNTIME`.

## Iterations 019–021 — Moving Link foot and dual-clip phase

- `7779c521a6e2d46209d075707faf5e902016968a` replaces the iterative boot-mesh penetration solve with the real `footBgCheck` foot/toe sampling boundary. The tick-30 knee jump disappears and divergences fall from 9642 to 9609.
- `488b5621aa9bde6046f4e97914da968ade8c50d8` preserves the unnormalised `JMAQuatLerp` result across recursive old-frame morph updates.
- `d92540fc39372e555b21ffac3d97dcdf0b93b8a3` removes the DTRC observation offset from `commonDoubleAnime` secondary-slot phase transfer. Divergences fall from 9609 to 9592 and the shared frontier advances from tick 30 to tick 31.
- current parity build ID: `sha256:fad41851c77e03d54344711d55a2ea24166f1af5c1ef03a48704a95e390436b4`; native markers and metrics are valid for `link_walk`, `link_run`, `link_stop`, and `link_collision_wall`.
- local classification: `DPAN_V1_HERMITE_DATA_LOSS`. DPAN v1 contains only integer quaternion poses, while `J3DAnmTransformKey::calcTransform` evaluates BCK s16 rotation, translation, and scale curves with Hermite tangents at fractional frames. No approximate oracle fit or tolerance increase was applied.

## Iterations 022–023 — Wait-turn dispatch and double-controller entry

- `ef94879eecfb3a99dad7aa664849e34033309e65` routes `TurnInPlace` through the source single-animation wait-turn path. Resource `0x233` and frames `0`, `0.7`, `1.4`, `2.1`, `2.8` align through tick 34; divergences fall from 4291 to 4212.
- `5315bf1852528219c72f5f42f6d96679b84fedbd` reproduces the `commonDoubleAnime` entry rule: after a single-animation controller, `field_0x2f8c == 0` starts both double-controller clips at frame zero.
- parity build ID: `sha256:eec1794c7131def243a67dce412019787aaf9bef6385da0a1777427a9b4ee310`; broker request `20260801T185523Z-parity_trace-1` completed with valid native marker and metrics.
- result: divergences fall from 4212 to 3432 and first causal divergence moves from `ANIMATION_RUNTIME` to `GROUNDING` at tick 35.

## Iterations 024–029 — Pre-ground state, input angle, and yaw semantics

- `8d48771f336f086075752607846ad2ca767c4771` reproduces source wait-turn exit acceleration. The first difference moves from reported `GROUNDING` at tick 35 to source speed at tick 37.
- `e315ee0bb27547a9b9016c0f65055798a629cb7e` implements the animated-foot contribution used by `setFootSpeed()` and `posMove()`. `link_turn_180` falls from 3431 to 1468 divergences and the first reported joint difference moves to tick 36.
- `b18ce1834c5e8ee441891df7a511612ac13aa811` composes the JUT stick angle with `dCam_getControledAngleY()` using signed-16 semantics.
- `c3061c701314e49b08088f931313f52c4c8f9d77` bounds causal comparison by each manifest's `maximum_ticks`; `link_idle_full_cycle` and `link_ground_contact` close.
- `4bbf64827b1aedd91aeddc792114286e78988ad4` matches `JUTGamePad::CStick::update()` angle conversion; `link_slope` falls from 20096 to 19833 divergences.
- `7849fc42bd68b46f0e7b4eb919bbcd06cbfe1282` uses integer `cLib_addCalcAngleS` movement-yaw chase; `link_slope` falls to 19823 and its first reported difference moves to a joint reference at tick 31.
- current parity build ID: `sha256:8264de147b259b5f918960afc8a817252800c073f750e2a8dd33c72716ce75d3`; 10/10 broker traces have valid boot, markers, metrics, and isolated profiles.
- current Link set: 2/10 match within tolerance; eight moving scenarios first report a joint-reference mismatch at tick 30, 31, or 36.
- local blocker: `DPAN_V1_HERMITE_DATA_LOSS`; no DPAN v2 data was fabricated and no grounding code, desktop oracle, tolerance, tick branch, or scenario branch was changed.
