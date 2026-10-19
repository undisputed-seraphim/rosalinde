# Unicorn Overlord data sources

Notes on where the game's data lives in `Unicorn.CPK`, gathered while mapping
characters/scenes. All paths are CPK-relative.

## Name / enum tables (authoritative, human-readable)

- `Debug/_UcEnum_*.inc` — generated enum headers, plain text:
  - `_UcEnum_CharaSet.inc` → `CHARASET` (1388 character definitions)
  - `_UcEnum_Class.inc` → `CLASSTYPE` (74 classes)
  - `_UcEnum_CharaNpc.inc` → `NPCTYPE` (320 NPCs)
  - plus Buff/Item/Skill/Facility/EquipAi/DropTable/QuestList/etc.
  - Format: `TABLE:count`, then `NAME , // localized label`, terminated by
    `TABLE_END`. Dump with `asbrun export enums`
    (writes `viewer/data/uc_enums.tsv`).
- `MsgSheet/*.fms` — FMSB string tables (localized text, 3 columns per row):
  - `UcCharaList.fms` (1388 rows) / `UcClassList.fms` (313) / `UcCharaNpcList.fms`
    → localized names only (no asset references).
  - `UcCharaColorList.fms` (2169 rows) → **color names** (`青`, `赤`, …), not
    per-character data.

## Character assets

- `Chara/<Class>_<M|F>NN.mbs` + `.ftx` — battle sprites, per class and gender
  (e.g. `Chara/Soldier_M.mbs`, `Chara/Cleric_HG_F00.ftx`).
- `CharaOp/Op_<Class>_<M|F>N.mbs` + `.ftx` — drama / cutscene sprites (what the
  `.asb` scene scripts reference).
- `Chara/<Class>.otm` and `OTM/Chara/<Class>_<PART>.otm` — `OTBM` object/part
  maps (magic `OTBM`, 120-byte header, embedded `<CLASS>.MBS` name). Parts are
  named like `_MOBM1..6`, `_MOBF1..6`, `_GLO`, `_HDL`, `_AMR`. Per **class**,
  not per named character. The `OTM/` tree also holds GUI/BattleBG/Effect
  variants.

## Engine database

- `DbReg/DbReg.drg`, `DbReg/DbRegDb.drg` — compiled tuning database (magic
  `DRG`). Contents are system parameters (font/color/sound/network/field/camera,
  sources like `Field/Camera.txt`, `Base/Sound.txt`). **No** `.mbs`/`.ftx`
  references, so not the character table.

## How scripts identify characters

Scene scripts create characters by **name**, never by a numeric appearance mask:

- `DrCreateChara(displayName, defName, int)` — e.g. `DrCreateChara("主人公","アレイン",0)`.
- `DrCreateNpc(name, ...)`, `DrUiCreateChara(ftx, mbs, dispName, charName, int)`.
- `StCreateUnit(defName, ...)` / `FlCreateCharaSymbol(...)`.

A scan of all 680 `Script/*.asb` found **none** of the viewer's `variant_flags`
values (`0x10000`, `0x20000`, `0x54010000`, `0x60030000`, …). The "flag" a scene
passes is the character **def name** (`PL_*`, `DRNPC_*`, `STNPC_*`, …), which the
engine resolves to class + appearance + colour.

## Resolution chains we can build

- **Drama**: `dispName`/`charName` → `CharaOp` asset, via the `DrUiCreateChara`
  calls found across the archive (`asbrun export chara` →
  `viewer/data/chara_table.tsv`).
- **Battle**: `class` → `Chara/<Class>_<M|F>` by naming convention; a character
  **def** → class by the class token inside the def name
  (`PL_FIGHTER_C0 → FIGHTER`).

## Open gap

Per-character appearance — the viewer's `variant_flags` mask over an MBS's
`s4.attribute` groups — is not present in the enum tables, the FMS files, or the
scene scripts. It is most likely engine-side character data (and/or the `OTBM`
part maps). Until it is found, that mask is the only part of the viewer's
hand-written character table that cannot be generated.

## Engine tables (exe `.data`, not the CPK)

`tools/dump_stage_tables.py` reads these from `main2.elf` with `.rela.dyn`
applied:

- **Stage table** `0x28920d0`, stride `0x50`, 103 entries: name `+0`, base level
  `+0xc`, country `+0x10`; **stage cue table** `0x285d9f8`, stride `0x10`, 103
  entries: name `+0`, cue `+8`. Names are `ST_OW_*` and match the 103
  `St_OW_*.asb` scripts. → `viewer/data/stages.tsv`
- **Field background catalog**: `FieldBG/Field.ufb` header `+0x60` is the asset
  count (39), followed by `main/texture` path pairs in the trailing string
  pool. The engine indexes the same pairs through `field_get_asset_record`
  (`0x20fa10`): `manager->assets + id * 0x10`. Field backgrounds are selected by
  the stage row's country and the stage-row variant, not by a script-name
  prefix. `FieldBG/Field.ufb` also supplies the `_00P`, `_00F`, and numbered
  field variants; the viewer catalog loads the exact pair names from this
  table.
- **Facility table** `0x27b31b0`, stride `0x90`, 485 entries: Japanese label `+8`,
  position `+0x1c/+0x20`; index-aligned with `ST_FACILITYID` (485 entries).
  → `viewer/data/facility_pos.tsv`
- **Drop tables** `0x26e5d30`, stride `0x18`, 733 entries: `start/count` at
  `+0x10/+0x14`; entries at `0xaee6d0`, stride `0x10`: item id `+0`, weight `+4`,
  count `+8`, add index `+0xc`. Joined with `_UcEnum_DropTable.inc` (733) and
  `_UcEnum_Item.inc` (983).
  → `viewer/data/drop_tables.tsv`

The field-BG window id is not a table (hardcoded lanes 0..3 → `0x116`, `0x117`,
`0x118`, `0x11a`).

**Country regions** are 22 quads built by the field-manager constructor
`0x202d00` (record stride `0x64`: country id `+0`, points `+0x44..+0x60`).
`tools/dump_country_regions.py` emulates the constructor (requires `unicorn`)
into `viewer/data/country_regions.tsv`; the engine's 5-entry id map is
`3,4,1,0,0` and an unmatched point returns `2`.

**Field cues** are a hardcoded nation map in `0x2f8620`
(`ゼノイラ`/`エルヘイム`/`バストリアス`/`ドラケンガルド`; default `104/105`),
backed by the cue table at `0x2400478` (stride `0x14`; day `+4`, night `+8`).
