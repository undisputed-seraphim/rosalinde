# COMBAT — Combat / Stage system plan

**Status:** deferred design. No combat rules are implemented yet. This document is the
single reference for the later combat-focused implementation phase.

The current `St*` handlers, battle callbacks and combat-facing field actors are
deliberately stubs/state-only. They remain annotated with `TODO(combat)` until this
plan is executed. The callback registry already stores battle callbacks; dispatch is
deferred.

## Scope

### Battle / stage native surface

The core native surface is grouped as follows:

| Group | Bare natives |
|---|---:|
| Battle scene / messages / timers | 20 |
| Zouen / reinforcements | 11 |
| Gambit | 2 |
| Selectable / targetable / commands | 9 |
| Army / station / partner | 7 |
| Stage rects / map / AI defaults | 8 |
| AI configuration / movement | 42 |
| Facilities / gimmicks / vehicles / regions | 17 |
| Unit lifecycle / queries | 39 |
| Misc (`Bt*`, `AddBattle*`) | 11 |
| **Total** | **166 bare names / 168 signatures** |

Additional field-combat natives:

- `FlCreateEnemy`
- `FlCreateChaseEnemy`
- `FlCreateVehicle`
- `FlSetBattleMode`
- `FlSetBattleAfter`
- `FlSetBattleDieAfter`

Already represented in the groups above: `AddBattleFlag`, `StSetStationItem`,
`StSetFacilityDir`, `UcStChaseParam::SetObjectName`.

### Scripted battle surface

These are **not shimmed**. They are module code executed by the VM; the combat
implementation must make their native calls return correct values and fire callbacks.

- `AiLib::St*` — 12 battle-AI helpers.
- `Lib::St` — 21 battle/stage helpers.
- `StOwCommon` — 65 battle-common helpers.
- Module-local functions in battle/stage scripts (`St_*`, `FT_*`, `Bt*`, `Ai*`) —
  about 2,369 functions in the battle subset.

Full lists: `docs/API_SCRIPTS.tsv`, `docs/API_NATIVES.tsv`.

### Outside this track

Keep these in field/non-combat work unless later shown to feed encounters:

- `FlAddSymbolAi*`, `AiLib::Fl*` — field symbol AI.
- `FlCreateChaseChara` / `FlCreateFleeChara` — field chase/flee AI.
- `FlCreateCharaSymbol` — field symbol creation; asset resolution is already fixed.
- `FlGetBreakthroughRate` — map-completion query, not combat.
- `StAddTalkEvent`, `StSetFacilityEvent`, `StSetGateBreakAfter`,
  `FlSetFacilityAfter`, `FlAddTownLvUpEvent` — field interaction callbacks.
- Item/UI/audio/gradient work.

## Architecture

### One authoritative combat model

Add a `CombatContext` / `CombatSystem` under `scarlett/src/combat/` later. It owns:

```text
units, armies, facilities, stations
timers, stage config, battle scene/BG
selectable/targetable sets, commands
event queue, callbacks, deterministic RNG, frame, phase
```

`Registry::dispatch_battle` becomes a thin adapter that delegates to this model.
The current `_battle_units`, `_battle_timers`, `_unit_station`, `_facility_army`,
and related maps are temporary scaffolding and are migrated group by group.

### Model groups

- `Unit`: object name, chara def, class, army, HP/max HP, stamina, brave, state,
  position/facing, station/vehicle, flags, AI component list, skills/equipment,
  leader/demo/vehicle flags.
- `Army`: side, leader, units, partner.
- `Facility`: type, army, position, station points, gate state, event callback,
  drop table, garrison.
- `GambitRule`: target selector/type/state, skill, interval, limit, wait.
- `ZouenWave`: facility, max, interval, offset, unit sets, spot, AI flags.
- `Timer`, `StageConfig`, `BattleScene`.
- `CombatEvent`: battle-before/start, turn start, action, death, engagement,
  reinforcement, battle-after, encounter-before/after.

### Native adapters

Every combat native gets one mapping:

- config natives mutate the model;
- query natives read the model and return real values;
- action natives enqueue events or advance the model;
- option natives store flags;
- callbacks register in the existing callback store.

No combat rules in `viewer`.

### Scripted API

Do not reimplement `kind m`. The VM executes it. The combat system must provide:

- correct native return values (no defaults that change branches);
- real object creation/queries;
- event sources;
- callback invocation with correct arguments/returns.

### Callback dispatch

Reuse the current `ScriptEvent` store. Battle events:

| Event | Callback shape |
|---|---|
| `BattleBefore` | `bool()` |
| `BattleDie` | `void()` |
| `EncounterBefore` / `EncounterAfter` | `bool(string, string)` |
| `BattleAfter` | `bool(bool)` |
| `BattleDieAfter` | `bool()` |

Combat phase adds:

- a context-pool scheduler for callbacks that call `Wait`/`Msg`;
- callback firing at model transitions;
- argument/return specs per event.

### Renderer

The combat view consumes the authoritative model or a dedicated combat snapshot:

- units, facilities, gimmicks, vehicles;
- battle BG, stage rects, map image, coat-of-arms;
- selection/target indicators, commands, result messages;
- night mask/miasma;
- faction colors/status icons.

Keep this passive; it is the combat half of the remaining visual work and is
cross-referenced from the main guide, not duplicated.

## Work breakdown

### C0 — Freeze and inventory

- Freeze the native/scripted lists above.
- Keep current handlers as `TODO(combat)` stubs.
- Keep combat APIs out of the non-combat surface.
- Add this document as the single combat reference.

### C1 — Combat model skeleton

- `CombatContext`, `Unit`, `Facility`, `Army`, `GambitRule`, `ZouenWave`.
- `CombatSystem` dispatch surface.
- Native adapters delegate; no simulation yet.
- Exact getters return model values; no default zero.

### C2 — Data and RE

- Unit-set table.
- Class/unit stats, skills, equipment.
- AI component parameter tables.
- Gambit target selectors.
- Zouen wave tables.
- Stage rects/map/COA/battle BG.
- Facility/gimmick/vehicle types.
- Dump to `viewer/data/combat_*.tsv`.
- Use Ghidra/radare2 and the `docs/EXE.md` relocation/registration recipe.

### C3 — AI and movement

- AI component list execution.
- Move/straight/wait/turn/search/chase/board/conquer.
- Default action flags.
- Spatial queries: rects, circles, search areas.
- `StStation` position/list sync.
- Move speed and deterministic RNG.

### C4 — Turn loop and resolution

- Unit lifecycle/stats from data.
- Turn/action/end.
- Engagement/damage/HP/stamina/brave.
- Gambit execution.
- Zouen waves/reinforcements.
- Victory/loss, timers, stage clock, battle messages.
- `AddBattleFlag` state.

### C5 — Event and callback dispatch

- Combat event queue.
- Context-pool coroutine scheduler.
- Fire battle callbacks with correct arguments/returns.
- Synthetic trigger tests before real battles.

### C6 — Combat rendering

- Units/facilities/gimmicks/vehicles.
- Battle BG/stage overlay/map/COA.
- Selection/target/command UI.
- Result messages, night/miasma.
- Deterministic battle captures.
- Link to the remaining field/visual work.

### C7 — Tests and tuning

- Unit tests for model/AI/turn/combat math.
- Script callback tests.
- Integration against selected `St_*`/`FT_*` scripts.
- Visual regression for combat views.
- Data validation (counts/ranges/references).

### C8 — Exit criteria

Combat phase is done when:

- no combat native remains a `TODO(combat)` no-op;
- all combat getters return model values;
- battle callbacks fire with correct arguments;
- a battle script can run through entry → turns → result deterministically;
- combat views render units, stage, selection and result UI;
- tests/captures pass.

## Risks

- Combat formulas and AI parameters are largely unrecovered.
- Callbacks are coroutines; simple synchronous invocation is insufficient.
- Player/event sources do not exist yet.
- Live-model migration may conflict with the current `Timeline` recorder.
- Save/party stats feed combat; the save model may need to land first or be stubbed
  consistently.
- Field AI and combat AI must stay separated to avoid scope creep.

## Open questions

1. Include `FlCreateChaseChara`/`FlCreateFleeChara` in combat, or keep them in field AI?
2. Include combat rendering in this track, or keep it with the remaining visual work?
3. Extract C2 data before C1, or build the model against placeholder tables first?
4. Build a minimal deterministic battle harness in C1/C5 before full AI?

## References

- `AGENTS.md` — build, environment, and runtime facts.
- `docs/API_NATIVES.tsv` — every native declaration and handler.
- `docs/API_SCRIPTS.tsv` — every script declaration, group, and count.
- `docs/EXE.md` — engine tables and native registration.
- `viewer/data/stages.tsv`, `facility_pos.tsv`, `drop_tables.tsv` — existing data.
