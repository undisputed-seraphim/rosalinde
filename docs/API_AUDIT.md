# Native & scripted API audit
Scope: every engine-native (`kind a`) and module/script (`kind m`) declaration used by the **680** `.asb` scripts in `Unicorn.CPK`. This is a static, code-backed audit, not a runtime census. Callback bookkeeping for gap 1 has since landed; the rest is the pre-change baseline.

> **Baseline note:** the live `RuntimeSession`/`SceneState` path and design-space
> `SpriteMone` rendering have landed since this audit. Use the counts and native
> coverage here; the "record-only"/"no consumer" wiring notes describe the baseline,
> not the current renderer.
Artifacts:
- `docs/API_NATIVES.tsv` — all **686** distinct native declarations, handler line, semantic status, renderer wiring, notes.
- `docs/API_SCRIPTS.tsv` — all **4,025** distinct script declarations with script/call counts and group.
- This file — method, counts, renderer map, and the final gap list.

> The native TSV is a pre-bookkeeping snapshot for the 16 callback registrations; those are now stored (gap 1).
## How the list was extracted

The baseline sweep was produced by the legacy ASB harvest pass (`asb_harvest --all`);
the current equivalent is `asbrun scan --summary --json`. For each module the harvest
emits `callpool` from `Module::used_functions` with the serialized
`used_function_kind`. `a` = engine native; `m` = function compiled into the
module/script. `functions[].calls` are indices into that pool, so the sweep gives
distinct declarations, scripts importing them, and call sites. One-off audit scripts
consumed the sweep; they were not kept in the repo.
`docs/DEFERRED_APIS.md` is a **name-coverage** check only: it matches a bare native name against a `name ==/rfind/starts_with` condition in `registry.cpp`. That is why it reports 0 deferred even though many handlers are no-ops, state-only, or record-only. This audit checks the handler body and the viewer consumer.
## Counts
| item | count |
|---|---:|
| `.asb` modules | 680 |
| distinct `kind a` declarations | 686 |
| native names / bare names | 654 / 644 |
| engine/value-type declarations | 656 |
| VM/add-on/internal declarations (`$beh*`, `$fact`, `$list`, `string::*`, `opIndex`, `length`, `$obj`) | 30 |
| distinct `kind m` declarations | 4,025 |
| script names / bare names | 4,012 / 2,017 |
| script namespaces | 127 |
| unmatched engine native names | **0** |
| native declarations with a registry condition line | 663 |
| add-on/internal declarations without a registry condition | 23 |

The 686/656/30 split is the useful one: 30 declarations are AngelScript behaviour/factory/add-on internals, not engine services. Every one of the remaining 656 has a name match somewhere in `registry.cpp`/`host.cpp`; the static `DEFERRED_APIS` claim of 0 is therefore correct at name level, but **not** a claim that the call has an effect.
## Native status (handler-body classification)
| status | count | meaning |
|---|---:|---|
| `live` | 109 | writes entity/camera/effect/light/fade/shake/bokashi/variation/fog state the viewer consumes |
| `live+record` | 1 | live writes plus a record-only channel |
| `record-only` | 131 | writes a `Timeline` channel the viewer does **not** consume (texts/UI/field/items/audio/stages/gradients) |
| `state-only` | 212 | writes only shim/save/battle/text-parameter state; no render path |
| `no-op` | 98 | body is effectively `return true;` |
| `query/stub` | 64 | returns state or a default/constant |
| `query` | 13 | computed query (tables/timing) |
| `deferred/approx` | 15 | explicit TODO/approximation |
| `scheduler` | 13 | Wait/park hook |
| `add-on/internal` | 30 | VM/add-on behaviour/factory, not an engine service |

Counts are exclusive handler classifications. They are deliberately conservative: a handler that creates an entity but the viewer later ignores the mutated fields still counts as `live` here and is separately listed under “ignored entity state”.
### By group
| group | decls | live | record-only | state-only | no-op | query/stub+query | deferred | scheduler | add-on |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| UI/archive/HUD | 25 | 0 | 19 | 3 | 0 | 0 | 3 | 0 | 0 |
| audio/timing | 15 | 0 | 13 | 0 | 0 | 2 | 0 | 0 | 0 |
| battle/stage | 180 | 1 | 6 | 51 | 97 | 20 | 4 | 1 | 0 |
| drama/cutscene | 85 | 61 | 4 | 8 | 0 | 7 | 3 | 2 | 0 |
| field/overworld | 92 | 11 | 50 | 24 | 0 | 6 | 1 | 0 | 0 |
| item/treasure | 5 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 |
| math | 4 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 |
| misc/debug/platform | 88 | 19 | 3 | 27 | 0 | 6 | 0 | 10 | 23 |
| state/save/data | 129 | 18 | 16 | 65 | 0 | 26 | 4 | 0 | 0 |
| text/UI | 22 | 0 | 15 | 5 | 0 | 2 | 0 | 0 | 0 |
| value/add-on: $obj | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| value/add-on: FnVector2 | 13 | 0 | 0 | 9 | 0 | 4 | 0 | 0 | 0 |
| value/add-on: FnVector3 | 4 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| value/add-on: UcMsgBalloonParam | 6 | 0 | 0 | 6 | 0 | 0 | 0 | 0 | 0 |
| value/add-on: UcSpriteTextParam | 10 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 |
| value/add-on: UcStChaseParam | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| value/add-on: string | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |

## Renderer wiring map
`RuntimeState` consumes only these `Timeline` channels:
- `keyframes` → entities/camera (`apply_keyframe`); entity fields actually used: `pos`, `scale`, `track_index`, `track`, `alpha`, `dir_x`, `visible`, `layer_mask`, `color`, `sort_id_offset`, `dlprio`, `window_id_offset`, `prerender_force`, `order`, `parent`.
- `fades`, `effects`, `point_lights`, `shakes`, `fog`, `bokashi`, `variations`.

These channels are recorded but have **no viewer consumer**:
- `texts` — dialogue/text
- `ui` — archive/HUD/shop UI
- `field` — field objects/regions/portals
- `gradients` — gradient maps/regions
- `items` — item/treasure visuals
- `audio` — low-priority audio playback
- `stages` — stage/weather/COA/flag configuration

Entity fields recorded but ignored by the viewer:
`layer`, `space` (UI), `reverse`, `breath_off`, `lipsync`, `shadow_off`, `state`, `state_icon`, `height`, `emote`, `search_flag`, `paused`, `auto_dir_off`, `reverse_x`. Also, a mid-scene `mbs` swap changes the final keyframe asset but the layer is built once from the final keyframe; per-frame asset swapping is not modelled.
## Script API

All `kind m` declarations are compiled into their module and executed by the stock AngelScript VM. They need no registry shim; the host registers only `kind a`. The full list is `docs/API_SCRIPTS.tsv`.

| group | declarations |
|---|---:|
| `FL_OW_*` facility modules | 2,153 |
| global/module-local | 1,410 |
| `Lib::` | 112 |
| `FlOwFacilityLib::` | 93 |
| `SEARCHPOINT` | 78 |
| `StOwCommon` | 65 |
| `DrLib::` | 48 |
| `FlOwTips` | 30 |
| `AiLib::` | 18 |
| `FriendshipLib::` | 8 |
| `FlOwTutorLib::` | 5 |
| other namespaces | 5 |
| **total** | **4,025** |

Scripted API *coverage* is therefore complete at the VM level (no missing shim); callback **dispatch** is deferred: see gap 1. The 680 modules decode/load/run per the existing `scarlett` sweep; this audit did not add a new execution sweep.

## Final gap list
### 1. Scripted callback dispatch (bookkeeping done, events deferred)
Registration/bookkeeping is now implemented: callback names are retained, resolved (including namespaced targets), and exposed in `Timeline.callbacks` / `asbrun record` JSON. Actual invocation is still deferred until the corresponding battle/field event exists:
- **Stored registrations (16):** `AddBattleBefore`, `AddBattleDie`, `StAddEncountBefore`, `StAddEncountAfter`, `StAddTalkEvent`, `StDeleteTalkEvent`, `StSetFacilityEvent`, `StSetGateBreakAfter`, `FlAddTownLvUpEvent`, `FlSetBattleAfter`, `FlSetBattleDieAfter`, `FlSetFacilityAfter`, `CreateTreasure`, `FlCreateChaseEnemy`, `FlCreatePortal`, `FlAddPort`.
- `AddBattleFlag`, `StSetStationItem`, and `StSetFacilityDir` are not callbacks; they remain state/data work.
- **Data not retained:** `StSetStationItem` ignores the drop-table id; `StSetFacilityDir`/`StSetStationItem`/most `StCreateGimmick` family are no-ops.

Impact examples: `StAddEncountAfter` 100 scripts/115 calls; `StSetFacilityEvent` 103/178; `AddBattleBefore` 51/204; `StAddTalkEvent` 12/31. The script API is VM-executable and the registrations are retained; dispatch is deferred with the event source.
### 2. Native objects that never reach the renderer
**Battle/stage:** `StCreateUnit`, `StCreateTempUnit`, `StCreateEntryUnit`, `StCreateDemoUnit`, `StCreateVehicle` write `_battle_units` only — no `RuntimeEntity`, so units never render. `StCreateFacility`/`StCreateFacility_DefName` only insert a name; `StCreateGimmick`, `StCreateFire`, `StCreateCloud`, `StCreateCloudGenerator`, `StCreateCircleRegionObject`, `StCreateLookoutRegionObject`, `StCreateSwitchObject`, `StCreateFacility_StageRect` are no-ops. `StSetBattleScene`/`StSetBattleBG` state only. `StSetStageRect`/`StageRectA`/`StageMapImage*`/`StageCoatOfArms`/`StageAi`/`UnitStageAi` are no-ops. Battle spatial queries (`StIsArmyInSearchArea*`, `StIsUnitInSearchArea*`, `StIsUnitInRect*`, `StIsMoving`, `StIsConquerFacility`, `StGetSkillUseNum`, `StGetBoardingArmyType`) return defaults.
**Field:** `FlCreateEnemy` (356 scripts/7,889 calls) creates an entity but passes S(1) — the Japanese chara name seen in scripts, e.g. `ホプリタイ` — as the **MBS path**; it cannot resolve. `FlCreateVehicle` (356/772) and `FlCreateChaseEnemy` (356/18,290) create entities with no MBS/FTX; they will be skipped by `build_layers`. `FlCreateChaseChara`/`FlCreateFleeChara`/`FlCreateCharaSymbol` do pass assets and should be checked per call.
`FlCreateStage`, `FlCreateExStage`, `FlCreatePortal`, `FlCreateCollision`, `FlCreateTalkPoint`, `FlCreateSearchPoint`, `FlCreateGatheringSpot`, `FlCreateBonFire`, `FlCreateFreeMap`, `FlAddPort`, `FlCreateGradientMapRegion` field records only; no visual consumer.
### 3. Recorded but unrendered channels
| channel | representative natives | key counts |
|---|---|---|
| texts | `Msg`, `MsgAsync`, `Dialog`, `SelectMsg`, `CreateSpriteText`, `Message` | `Msg` 662/154,523; `CreateSpriteText` 427/3,721 |
| ui | `OpenArchive`/`CloseArchive`, `OpenShopUI`, `OpenUnitInfoUI`, `CreateFacilityName`, `ShowFacilityInfo`, staff roll, trophies | `OpenArchive` 610/4,602 |
| field | field objects/regions/portals/weather/flags | `FlCreateChaseEnemy` 356/18,290; `FlCreateCharaSymbol` is an entity |
| gradients | `DrSetGradientMap`, `FlSetGradientMap`, gradient regions | `FlSetGradientMap` 356/726 |
| items | `CreateItem`, `CreateDbItem`, `DeleteItem`, `CreateTreasure` | `CreateTreasure` 357/12,841 |
| audio | `PlayBGM`, `PlaySE`, `StopBGM`, `StopSE`, volumes, `UC_SEC` | `PlayBGM` 522/847; `PlaySE` 449/1,422 |
| stages | `DrCreateStage` (weather/COA/flag), battle BG/stage rects | `DrCreateStage` 293/439 |
### 4. Entity state the viewer ignores
`DrSetBreathOff`, `DrSetLipSync`, `DrSetShadowOff`, `SetSpriteMoneReverse`, `SetSpriteMoneAnimationPaused` (pause), `CreateSpriteMone` (layer), `DrUiCreateChara` (UI space), `FlSetState` (356/56,479), `FlSetStateIcon` (7,527), `FlSetAutoDirOff` (6,203), `FlSetSearchFlag`, `FlSetHeight`, `FlEmote`, `FlPauseSymbol`/`FlUnpauseSymbol`, `FlSetMoveAiPermissionInDemo`, `FlSetReverseX`, `FlSetAnimation` (reverse only).
Also: `DrSetEffectPosV`/`DrSetEffectOffsetV` are routed into the **entity position** handler; effects are `RuntimeEffect` records, not entities, so these are no-ops for effects. `DrSetLayerMask` and `DrSetVariationType` are consumed but their bit→part mapping is approximate.
### 5. Live model / camera / stage
The design→screen projection matrix was unknown at audit time (`docs/ASB.md`, `docs/EXE.md`); `RuntimeState` used an orthographic design space and `fit`/`first`/`last`/`record` camera hacks. There was no live registry: the VM recorded a `Timeline`, then the viewer played it. The live path has since landed. `DrCreateStage`, battle BG, and stage rect/map/COA were recorded/state-only and not drawn at the time.
### 6. Already tracked as low-priority
- Battle/AI simulation: AI components, gambit, Zouen, turn loop, damage, stage timer, unit stats, spatial queries (`TODO(combat)`).
- Audio playback.
- Save format/save-derived getters.
- `UC_SEC` ×60 timing.
- `FlGetBreakthroughRate` returns 0.
- Approximations: jump axis, `MoveBack*`, `DrCopyCharaInfo` direction, demo camera, shake int-preset envelope, field-AI one-shot tween, staff-roll/message timing, layer/variation/tint mapping, light falloff.
### 7. Visual regression
No deterministic visual regression suite yet. Existing captures are ad hoc (`--scene-frame`, `--capture-frames`, viewer debug envs).
## Bottom line
The native/script work is **not** mostly wired to the renderer yet. Name coverage is 100% for engine natives, but the renderer consumes only 8 timeline channels; 7 are recorded-only. The largest concrete misses are: battle units/objects never become entities; field objects are mostly field records (with `FlCreateEnemy`/`FlCreateVehicle` asset bugs); text/UI/item/audio/gradient/stage records have no consumer; callback registrations are retained but never dispatched; and several entity fields (state/icons/height/emote/reverse/pause) are recorded but ignored. These are exactly the remaining visual targets, plus a small set of genuine native-wiring bugs (callbacks, effect position, field assets) that should be fixed before or alongside the field/UI rendering work.
