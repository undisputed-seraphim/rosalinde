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
    `TABLE_END`. Dump with `asb_harvest --cpk Unicorn.CPK --uc-enums`
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
  calls found across the archive (`asb_harvest --chara-table` →
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
