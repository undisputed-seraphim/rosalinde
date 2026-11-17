# TODO — character default colours and colour-ID mapping

## Problem statement

The viewer does not reproduce the game's default character colours. A world
character's live colour block is stored at `chara+0x2018` and normally comes from
a per-character stage-placement definition (`def+0x160`); only when no placement
definition exists does the game fall back to the CharaSet colour-preset row. The
viewer currently has no stage-placement data, so every character falls back to
that preset row.

That fallback is not sufficient. Alain and Lex share preset 7 in the table, and
Scarlett uses preset 39, but the game's reference frames show per-character
results that do not follow a single fixed `color_id → preset slot` order. The
viewer therefore needs both:

1. a way to obtain the authoritative per-character colour blocks, and
2. a correct rule for mapping `color_id` layers (cloth, hair, armour, accents,
   skin) onto those block colours.

The part/slot mapping has a working luminance-based fallback today, but the
authoritative colour source is still open. This file records the remaining
candidate paths and everything already ruled out.

Status: investigation only. The part/slot mapping has a working fallback; the
authoritative source of each character's default colour block is still open.

The current work is on branch `char_color`, at `fe3f776`. `d340935` restores the
preset-table/gradient-map work and `fe3f776` fixes the colour-ID slot mapping.

## Lead 1 — investigated, verdict

**Verdict: the lead is real as a mechanism, but it is a runtime mirror, not a
data source. It cannot be mined from the CPK or from a stage file.**

What is confirmed:

- `stage_get_data()` (`0x19574`) returns the runtime stage-data buffer. The
  buffer layout is the `UCGLOBALDATA.DAT` layout already documented by
  `tools/dump_stage_data.py`; the file is not in the CPK.
- The stage-data ctor `FUN_002c7244` initialises four placement tables of
  500 × `0x1d0` defs via `FUN_002bf670`. Every record starts with
  `charaset = -1`; nothing is baked into the executable.
- Table 1 (`stage_data+0x2af30`) is the player/party table used by
  `chara_create_from_charaset`. If a def matches, the whole record is copied,
  including the `0x58`-byte colour block at `def+0x160`; the script's preset
  argument is then ignored entirely.
- The table is populated at runtime:
  - `AddCharaSaveData(name, lv, classChange, nation)` → `FUN_002ee1a8` →
    `FUN_002c8828` inserts into `+0x2af30`. The argument pattern matches the
    registered signature exactly: level, class-change flag, nation.
  - Stage/encounter placement → `FUN_002c7a88` → `FUN_002c788c` inserts into
    tables 1–4 from `g_encounter_table` records.
- The colour block is driven by an index, not by per-character RGB:
  - `nation == -1` → `chara_apply_charaset`, i.e. the CharaSet `+0x10` preset
    (Alain/Lex = 7, Scarlett = 39).
  - otherwise → `chara_color_block_apply_variant(nation)`, with nations 1–4
    mapping to presets `{4, 1, 2, 3}` (`0xAEDB80`); anything else, including
    `0`/unknown, maps to **preset 5** (white/crimson).
- The `AddCharaSaveData` calls found in the CPK are the debug-only
  `DrLib::DebugClassChange` in `ScriptDrLib.ash` (pushed as
  `AddCharaSaveData(name, 4, false, -1)` for the whole initial party). It uses
  the CharaSet presets, so it does **not** reproduce the reference. No
  story/recruit call site is in the CPK.

Empirical test of the mechanism:

- In the viewer, Lex gets preset 7 (blue skirt) and Scarlett preset 39
  (orange-red dress).
- The references are crimson for both: Lex's skirt in `reference.JPG` and
  Scarlett's dress in `OW_C_0_Fl_Main.mp4` have G ≈ B, which preset 7/39 cannot
  produce (their blue channel is 0). The cloth textures are neutral: Lex's
  rendered skirt is `(20, 36, 80)`, an almost exact 0.46× scalar multiple of
  preset 7's `(44, 80, 169)`. So the skirt colour is the flat tint, not the
  texture, and presets 7/39 are the wrong tints for the reference.
- Rendering the same scenes with preset 5 (data-dir override only, nothing
  committed) reproduces both references: Lex red skirt/silver armour, Scarlett
  crimson dress/white blouse, Alain unchanged.
- That means the reference cast's live blocks really do differ from their
  CharaSet presets, exactly as lead 1 predicts.

Why the lead still does not solve the problem:

- The table is a live mirror of the party/save. `UCGLOBALDATA.DAT` holds stage
  objects and encounter placements, not a static per-character colour database.
- The real nation/variant values are passed by the story recruit path that
  fills the party, and that path is not in the CPK. The save (or a default-save
  template) is the other candidate carrier.
- The variant path also copies preset 5's eight part-asset bytes (all zero) over
  the block. Those bytes feed `UcSpriteMone_build_part_assets` (`0x52cfc4`),
  which is reached from the menu/portrait sprite paths, not (as far as we
  found) the world/drama chara path. The drama models are unaffected, but
  portraits could be. Open question.
- The gradient-map override (`SetCharaGradientMapCommon/Hair/Main/Sub`) is not
  the explanation here: only `Script/FT_ALN_MOL.asb` calls it in the whole CPK,
  and it targets demo mercenaries. Plain drama characters keep the flat tint
  (`Index == 0`).

So the actionable continuation is leads 2–4: find the per-character
nation/variant (party roster, story recruit data, or default save). Lead 1 gives
the container and proves the reference blocks differ, but it cannot be extracted
on its own. A viewer-only workaround would have to invent the missing
per-character variants; using preset 5 for everyone on the party path would
match the two references but is not evidence that it is universal.

### Ghidra names from this lead

Renamed in `unicorn_main`:

| address | name | role |
|---|---|---|
| `0x2c7244` | `stage_data_ctor` | initialise the runtime stage data / four def tables |
| `0x2bf670` | `chara_def_table_init` | clear 500 × `0x1d0` defs, `charaset = -1` |
| `0x2c06a0` | `chara_def_table_insert` | allocate a free def slot and copy the def |
| `0x2c0570` | `chara_def_table_commit` | commit a slot and update encounter/slot state |
| `0x2c7a88` | `stage_encounter_spawn_defs` | spawn encounter placements into tables 1–4 |
| `0x2c788c` | `chara_def_from_encounter_slot` | build a def from an encounter slot |
| `0x2c8828` | `chara_create_party_unit` | build a party def from CharaSet + colour variant |
| `0x2ee1a8` | `native_AddCharaSaveData` | VM native `AddCharaSaveData(name, lv, classChange, nation)` |
| `0x35ad0` | `chara_get_protagonist_color_block` | find `主人公` in table 1, return `def+0x160` |
| `0x2bf950` | `chara_def_find_by_label` | resolve a CharaSet label, then find its def |
| `0x1a9244` | `chara_color_variant_from_id` | map a unit/model id band to nation variant |
| `0x2c0ce0` | `chara_apply_appearance_variant` | apply variant-derived appearance/seed to a def |
| `0x2c16c0` | `stage_encounter_slots_init` | clear the stage's 0xa4 encounter slot records |
| `0x2caf40` | `stage_object_table_init` | clear a stage object table |
| `0x567688` | `unit_view_create` | build a renderer view from a unit record |
| `0x2c2d08` | `chara_color_block_clear_parts` | clear the block's part/gradient fields |
| `0x2c21d4` | `chara_desc_init_empty` | clear a chara desc / set id = -1 |
| `0x2c2460` | `chara_desc_apply_appearance` | overlay appearance bytes from CharaSet/encounter |
| `0x2c8384` | `party_roster_register` | set roster bits for a created unit |
| `0x28cf100` | `g_game_context` | global game singleton (`+0xf8` → stage data) |

Parameters were renamed to match: `charaset_id`, `color_variant`,
`class_change`, `nation`, `army_type`, `preset_override`.

`chara_color_variant_from_id` (`0x1a9244`) maps id bands to variants:

| input id | variant | preset |
|---|---|---|
| `< 0` | `-1` | (caller keeps CharaSet preset) |
| `0..0x8e` | `2` | 1 (blue/white) |
| `0x8f..0xe7` | `3` | 2 (yellow) |
| `0xe8..0x13d` | `4` | 3 (green) |
| `0x13e..0x18e` | `1` | 4 (purple) |
| `> 0x18e` | `0` | 5 (white/crimson) |

The input is `unit_record+0x1c4` on the menu sprite path; it is not yet confirmed
whether that is a CharaSet id, class id, or model id.

## Lead 2 — investigated, verdict

**Verdict: it goes somewhere, and it identifies the missing input: the
character's nation. The roster itself is still runtime state, but the colour
semantics are now static and confirmed.**

What is confirmed:

- The colour variant is the character's **nation**, not a per-character RGB
  value. `chara_create_party_unit(stage, charaset, level, variant, flag)`:
  - `variant == -1` → CharaSet `+0x10` preset;
  - `variant == 1..4` → presets `{4, 1, 2, 3}` (`0xAEDB80`);
  - anything else, including `0`/unknown → preset 5.
- The nation → preset mapping is visible directly in the CharaSet generic rows
  (`viewer/data/generated/charaset.tsv`):

  | nation | generic CharaSet preset | colours |
  |---|---|---|
  | Cornia (`コルニア軍`) | 1 | blue / white |
  | Drakenhold (`ドラケン`) | 2 | yellow |
  | Elheim (`エルヘイム軍`) | 3 | green |
  | Bastorias (`バストリアス軍`) | 4 | purple |
  | Albion (`アルビオン軍`) | 5 | **white / crimson** |
  | Zenoiran (`ゼノイラ軍`) | 6 | dark violet |

  Every generic row in each group uses that one preset (31/31, 22/22, 30/30,
  4/4, 18/18, 86/86), so this is a real nation palette, not coincidence.
- The reference cast fits the mapping exactly:
  - Alain is from Cornia → preset 1. Preset 1 is the same blue/white as his
    CharaSet preset 7, so his look is unchanged.
  - Lex is from Albion → preset 5 (white/crimson), which matches
    `reference.JPG`.
  - Scarlett is from Palevia/Albion → preset 5, which matches the crimson dress
    in `OW_C_0_Fl_Main.mp4`.
- `chara_color_variant_from_id` (`0x1a9244`) is the runtime expression of the
  same idea. It maps id bands to variants:

  | id band | variant | nation palette |
  |---|---|---|
  | `0..0x8e` | 2 | Cornia (preset 1) |
  | `0x8f..0xe7` | 3 | Drakenhold (preset 2) |
  | `0xe8..0x13d` | 4 | Elheim (preset 3) |
  | `0x13e..0x18e` | 1 | Bastorias (preset 4) |
  | `> 0x18e` | 0 | Albion/unknown (preset 5) |

  The bands cover 399 ids and the function is used by the portrait/menu path
  with `unit_record+0x1c4`. So the game groups character/portrait ids by nation
  and derives the palette from the id.

What the roster path does:

- `Lib::JoinChara` → `Lib::AddNewCharaSaveData` → `AddCharaSaveData(name, -1,
  false, -1)`. The story recruit wrapper therefore passes **-1**, i.e. keep the
  CharaSet preset.
- Facility/generic recruits omit the nation argument, so they take the script
  default `UC_NATION_UNKNOWN`. If that is `0`, they get preset 5 (crimson).
- `StOwCommon::InitDebugArmy` is called from `Stage_Setup` when
  `IsEmptyPlayerUnitList()` is true. It adds the whole initial cast with
  `AddCharaSaveData(name, -1, false, -1)`, so the fallback start is also
  CharaSet-preset based.
- Therefore the shipped CPK scripts do **not** assign nations to the main cast.
  The nation must come from the save/party record (the value behind
  `unit_record+0x1c4`) or from a default-party initializer. That is the
  remaining unknown, but it is now a single scalar per character rather than a
  colour-block format.

Consequence for a fix:

- If the nation per character is known, the colour block is simply
  `preset = nation_preset[nation]` and the luminance slot rule still selects the
  right parts inside that preset:
  - Cornia (1): blue cloth, white/silver armour;
  - Albion (5): crimson cloth, white/silver armour.
- Do **not** hardcode character names. The nation should come from the same
  data the game uses (character/portrait id band or save record). A small
  authored `charaset → nation` column is acceptable only if the executable
  cannot supply it, and must be justified from the reference/data.

### Party unit-set path (continuation)

The early party is not magic: the C_0 stage script adds it through unit sets.

- `UcUnitList.fms` has 2859 names, exactly the `g_encounter_table` count.
  Unit sets 100–103 are unnamed player sets.
- `St_OW_C_0::Stage_Setup` calls `AddUnitSaveData(100)`.
  `Facility_SortieLex` calls `AddUnitSaveData(101)` (Lex).
  `Main_SortieJoseph` calls `AddUnitSaveData(102)`.
- Unit set 100 = `[Alain (Charaset 2), Scarlett (4)]`, 101 = `[Lex (5)]`.
- All of these records have preset 0 at `+0x18`, so the placement path applies
  the **CharaSet preset** (Alain/Lex 7, Scarlett 39), not a per-character
  palette.
- The per-army jump-table case in `chara_def_from_encounter_slot` (`0x2c7954`)
  computes an **appearance** variant/seed only. The colour block still applies
  the encounter preset and then the CharaSet preset last.

So the normal early-party join path also yields blue Lex and orange Scarlett.
The reference default is not in the unit-set/encounter records. That leaves the
global-data template, the 10-slot party colour array, or the save-record default
as the remaining per-character colour carriers.

Renamed in this pass: `stage_unit_set_spawn` (`0x2c85d0`),
`stage_unit_set_place` (`0x2c84bc`), `unit_set_name_setup` (`0x5678fc`).

### 10-slot party colour array — investigated, not the world source

`chara_color_block_array_init` (`0x2c3090`) initialises ten 0x58-byte blocks at
`stage_data+0x139bc`, every slot preset 7. The array is the **character colour
customisation screen's edit buffer**, not the world character store:

- `stage_party_color_block` (`0x2c322c`) = `&array[index]`, index < 10.
- `stage_party_color_block_set` (`0x2c31e8`) copies a block into
  `array[index]`.
- Both are used only by the two colour-UI state machines
  (`FUN_00382e5c`, `FUN_00383180`), which read the selected party member's block
  and write the edited block back.
- No `DrCreateChara` / placement-def path reads or writes this array. The world
  character uses the placement defs at `+0x2af30`.

So this array confirms that per-character repainting is a separate late-game UI
path; it does not supply the pre-customisation defaults. The array's default
(preset 7) also cannot explain the red Lex / crimson Scarlett references.

Renamed: `stage_party_color_block`, `stage_party_color_block_set`.

Also noted:

- `UcCharaList.fms` (CPK) is only a name list; its FMS properties are 0, so it
  does not carry the nation.
- `CharaFace/` has only 155 class-based faces and `CharaImg/` has 22 full-body
  portraits, so the 399-id band table is not directly those asset lists.
- `party_roster_register` (`0x2c8384`) only sets roster bitfields; it does not
  store the nation.

## Lead 3 — investigated, verdict

**Verdict: the save is a red herring, as you said. The initial-save path is
now identified, and it shows the default block is not in the CPK; but it also
turned up a much better colour source for named characters: the `CharaOp`
victory models bake the canonical palette into the texture, while the
battle/drama class models are grey and palette-tinted.**

Initial-save / global-data path:

- `global_data_task_init` (`0x49e88`) configures the global-data file task:
  - `+0x5e0 = global_data_default_base()` (`0x2ED7010`),
  - `+0x5e8 = 0x310b8` (default-block size),
  - `+0x608 = "UCGLOBALDATA.DAT"`,
  - `+0x688 = global_data_version()`.
- `save_load_ui_update` (`0x56e7fc`) case 0 is the reset/new path:
  `global_data_clear_and_init()` then
  `global_data_reset_default_block(g_game_context+0x100)`.
- `global_data_reset_default_block` (`0x2bbaa4`) copies `0x310b8` bytes from
  `0x2ED7010` into the global data at `+0x31000`.
- Global data buffers:
  - current `0x2F080C8`, size `0x4E2410`;
  - backup `0x33EA4D8`;
  - clear/restore helpers `global_data_clear_and_init` /
    `global_data_restore_backup`.
- Per-slot story saves are separate: `save_slot_initialize_path` (`0x49b64`)
  builds `UCSAVEFILE##` paths; `save_slot_load` (`0x49d7c`) loads them.
  `UCGLOBALDATA.DAT` is the global-data template, not a story save.
- The `0x2ED7010` buffer is BSS/zero in the ELF, so a fresh build gets a zero
  default block unless `UCGLOBALDATA.DAT` supplies it. It is not in the CPK.
- The stage-data ctor also initialises a **10-slot party colour-block array** at
  `stage_data+0x139bc` (`chara_color_block_array_init`, `0x2c3090`), every slot
  defaulting to preset 7. That array is a likely carrier for per-party-member
  palettes; its writer was not found (the stage data is saved/restored as a
  whole).

The important colour discovery:

- `CharaOp/Op_*.ftx` are the victory/pose models. Their textures carry the
  **canonical character colours baked in**:
  - `CharaOp_Op_Fighter_M0.ftx` has red cloth, red hair, brown leather and
    silver armour;
  - the battle/drama `Chara/Fighter_M0.ftx` is grey and gets its colour from the
    preset tint (preset 7 → blue skirt).
- So Lex's red outfit is a real per-character default, not the generic Fighter
  nation/class palette. The reference matches the baked Op palette, not preset 7.
- `CharaOp_Op_Scarlet_F0.ftx` is mostly grey, so her red dress still comes from
  a per-character tint/vertex colour rather than the atlas. The Op MBS
  (`CharaOp/Op_Scarlet_F.mbs`) only has a `VICTORY_1` track, so it is not the
  drama model; it is a useful canonical-colour reference, not the runtime path.
- The viewer's world drama path resolves the CharaSet first (`_chara_sets` →
  class `Chara/` MBS) and only falls back to the character table (`CharaOp/`)
  when the class is unknown. So it never sees the Op baked colours for known
  characters.

Where this leaves the problem:

- The named-character default palette is per-character data, not the nation
  palette and not the CharaSet preset.
- It is not in the CPK scripts: the story recruit wrapper passes `-1`
  (CharaSet preset), and the only default path (`InitDebugArmy`) does the same.
- The remaining plausible carriers are the global-data template
  (`UCGLOBALDATA.DAT`), the 10-slot party colour array, or the
  `unit_record+0x1c4` id → variant path in the menu/save code.
- The next concrete step is to dump a shipped/first-boot `UCGLOBALDATA.DAT` or
  the 10-slot array after a known first meeting, and compare the resulting
  per-character blocks with the Op textures. A player save is only useful to
  extract the default template if it was written before any repaint.

NSP note: the program NCA is `Titlekey crypto` (rights ID
`010069401ADB80000000000000000011`, master key revision `0x10`). The ticket's
title key did not decrypt section 0 with the available `titlekek` values, so the
RomFS was not enumerated here. The CPK was already extracted separately; if the
default template is shipped, it would have to be pulled from that RomFS/save
partition.

Ghidra names added in this lead:

| address | name | role |
|---|---|---|
| `0x49e88` | `global_data_task_init` | global-data file task (`UCGLOBALDATA.DAT`, default block, size) |
| `0x49fa8` | `save_slot_any_initialized` | checks the 12 save-slot headers |
| `0x2bf9d0` | `chara_def_find_by_unit_id` | find a placement def by `def+0x1c4` unit id |
| `0x2bed1c` | `chara_def_ctor` | construct/reset a 0x1d0 chara def |
| `0x2beddc` | `chara_def_reset` | reset the def fields used by the fallback |
| `0x2c0dac` | `chara_def_apply_class` | apply class/level scaling to a def |

## 1–8: candidate sources for the original/default colours

### 1. Stage placement character definitions (primary)

`chara_create_from_charaset` (`0x175be0`) first looks up the CharaSet id in the
stage placement table:

```
chara_def_find_by_charaset(stage_data_get_placement_table(stage_data, 1), charaset_id)
```

If a record is found, the whole `0x1d0`-byte character definition is copied,
including the `0x58`-byte colour block at `def+0x160`. Only when no placement
record exists does it fall back to `chara_apply_charaset` and the CharaSet
`+0x10` preset.

- placement table: `stage_data + 0x2af30`, 500 records of `0x1d0`
- CharaSet id: `def+0x34`
- colour block: `def+0x160`

Open: where the stage data for a drama scene is loaded from. This is the most
credible explanation for Scarlett's block differing from the raw preset row.

### 2. Party/unit roster defaults

`FUN_002c8828` creates field/party units. It either uses the CharaSet preset or
`chara_color_block_apply_variant(param_4)`; the variant table is
`chara_color_variant_presets` (`0xaedb80`), `{4, 1, 2, 3}`.

Open: the default party/member records, their `colorVariant`, and whether they
carry saved colour blocks. The stage placement path may be populated from these
records before a drama scene starts.

### 3. Save-slot/default-save template

`save_slot_apply_stage_data` (`0x49d34`) calls `stage_data_set_slot`; the
placement table is therefore also a save-slot object. Even if a player save is
irrelevant, a **new-game/default save template** may contain the initial
per-character colour blocks.

Open: find the default save/stage-data initializer and its colour blocks.

### 4. `UcCharaColorList.FMS` plus `DrSetCharaColor` indices

`DrSetCharaColor(object, common, hair, main, sub)` takes indices into
`MsgSheet/UcCharaColorList.FMS` (2169 strings), not RGB values. Only
`Dr_OW_Z_Epilog_0_A.asb` calls it, so the defaults are not scripted.

Open: the index → preset/colour mapping and every character's default indices.
The default indices may live in CharaSet/CharaDef fields that have not been
identified yet.

### 5. Encounter-derived colour blocks

`chara_color_block_apply_encounter` (`0x2c2f5c`) can apply a preset from
`record+0x18` and then an override from `record+0x10`. Stage placement records
may be generated from encounter records via `FUN_002c84bc` / `FUN_002c85d0`.

Open: whether the C_0 cast has encounter records with default colour data.

### 6. Special-appearance fixups

`chara_color_block_fixup_special_appearance` (`0x3950c`) replaces the block with
preset 7 when appearance bytes match presets `0x35/0x36/0x37`. It was found in
the menu-sprite path (`UcSpriteMone_set_color_block`).

Open: whether a world-chara equivalent exists, or whether this explains some of
the observed slot reordering.

### 7. Material/consumer trace

The block reaches the render object through the track-instance material:

```
chara+0x2018  →  track-instance material (+0x20)
              →  render_object_set_material (+0x80..+0xe0)
              →  sprite_layer_draw
```

Open: whether the engine reorders block slots, selects by luminance, or
multiplies the preset by the layer's `section_0` colour. This is the best way to
confirm or replace the current luminance fallback.

### 8. CharaSet row fields not yet explained

`w10` high 32 bits, `w18` high bytes, `w0c`, and `w38` are not understood. They
may encode the default colour variant, colour-list indices, or a slot
permutation.

Examples:

| row | character | w10 | w18 | w0c | w38 |
|---|---|---|---|---|---|
| 2 | Alain (`主人公`) | `0x100000007` | `0x90900000005` | 4 | `0x3bf` |
| 4 | Scarlett (`高貴プリーステス`) | `0x200000027` | `0x50700000000a` | 82 | `0x33a` |
| 5 | Lex (`せっかちファイター`) | `0x700000007` | `0x201000000006` | 53 | `0x2830000` |

Open: test these fields against the reference/party data before assigning any
meaning.

## Current solution: character colour-ID slot mapping

Implemented in `viewer/src/sprite.cpp` (`preset_slot_for_color`,
`color_uses_preset`), used by both the render path and the probe path.

Observed layer roles:

| `color_id` | role |
|---|---|
| 1 | common cloth |
| 2 | hair |
| 3 | main garment / armour |
| 4 | armour accent |
| 5 | skin / hand |
| 10+ | untinted / other |

Current mapping:

```
color_id 1 (common cloth)  → darker of colour0 / colour1
color_id 3 (main garment)  → lighter of colour0 / colour1
color_id 2 (hair)          → untinted (section_0 / atlas colour)
color_id 4 (armour accent) → untinted for now
color_id 5 (skin/hand)     → untinted for now
```

Why: preset rows do not keep the two communal colours in a fixed order.

- preset 7 (Alain): blue / white → blue cloth, white armour
- preset 39 (Scarlett): white / red → white blouse, red dress

A fixed `1 → colour0, 3 → colour1` mapping gave Alain the right result but made
Scarlett's blouse red and dress white. A fixed `1 → colour1, 3 → colour0` did the
reverse. The luminance ordering matches both known references.

### Searched and ruled out for this mapping

- **Fixed slot mapping.** Ruled out: preset 7 and preset 39 have opposite
  colour0/colour1 ordering.
- **Byte order.** Ruled out: `FUN_0058b384` reads the low byte as R, matching the
  viewer's vertex format. Preset 7 is genuinely blue/white.
- **Gender-based swap.** Tested: female mapping made Scarlett correct and male
  mapping kept Alain correct, but the preset table's white-first/white-second
  ordering is not gender-correlated, so gender is not the underlying rule.
- **Tinting hair with the preset.** Ruled out: the FTX atlas already carries the
  hair tone. Tinting `color_id 2` turned Lex's and Scarlett's hair the wrong
  colour.
- **Mapping `color_id 4/5`.** Left unmapped: `4` looks like a second armour
  pass; `5` must keep the skin colour because preset `colour3` is not a skin
  tone.
- **Per-character preset lookup.** Ruled out for the preset table itself:
  `chara_color_get_preset` is a straight `0xCEAB20 + index*0x18` lookup.

Risk: the luminance ordering is inferred from two references. It is a fallback,
not proven engine behaviour. More references may show that a different
per-character block is the real source and the fallback is unnecessary.

## Already searched and ruled out for the original colour source

- **CharaSet name resolution.** Not a bug. `DrCreateChara` takes
  `(CharaSet label, object name, preset)`. Alain resolves to `PL_LORD_C0`
  (preset 7), Scarlett to `PL_PRIESTES_C0` (preset 39), Lex to
  `PL_FIGHTER_C0` (preset 7).
- **Preset byte order.** Not a bug; the engine and viewer both use `0xAABBGGRR`.
- **Per-character static colour table.** None found. The known palette words
  exist only in `g_chara_color_presets` (`0xCEAB20`).
- **Scripted defaults.** No script sets default character colours.
  `DrSetCharaColor` appears only in `Dr_OW_Z_Epilog_0_A.asb`.
- **CPK stage data.** No `.stg`/stage-data resource exists in the 4,752-entry
  CPK. `StBtShore.bbs` consumes exactly to EOF and has no character defs.
- **Palette bytes in the CPK.** No occurrence of the preset palette bytes in the
  unpacked archive.
- **`STAGEDATA_BINARY_RESOURCE` loader.** The string exists at `0xAE65F8`, but
  no live reference/vtable call path was found; treated as a dead end for this
  build.
- **BBS backgrounds.** `StBtShore.bbs` is a background object table only.
- **NSP.** Nintendo package metadata/NCAs; not pursued.
- **CharaSet `w48`/class table.** `chara_apply_color_block` copies class `w48`
  to `chara+0x200c`, but no colour consumer was found.
- **`chara_color_block_apply_variant` / `apply_encounter` for drama.** These are
  used by field/party or encounter paths, not the drama `DrCreateChara` path as
  currently resolved.

## Verification for `fe3f776`

- `Dr_OW_C_0_St_Before_0` frame 180: Alain blue cloth, silver armour.
- `Dr_OW_C_0_Fl_Main_0` frame 900: Scarlett blonde, white blouse, red dress.
- `Dr_OW_0_Opening` frame 420: pixel-identical.
- `scarlett_test`: 201 assertions / 23 cases.
- `criware_test`: 1/1.
- Extractor: 4,752 entries.
- 23-scene headless sweep: clean.

Lex's cloth remains preset-7 blue; his reference/default block is still missing.
