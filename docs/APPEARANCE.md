# Character appearance (CharaSet → class/gender/variant)

How `viewer/data/variants.tsv` is produced, and why it no longer needs to be
hand-written. See `docs/ASB.md` for the wider drama/script picture.

## The problem

The viewer draws an MBS layer when `(layer.attributes & ~variant_flags) == 0`
(`sprite.cpp`). `variant_flags` is the set of `s4.attributes` bits a character
enables. It was originally a hand-written table (`variants.tsv`) because the
game's own selection could not be found in any CPK file.

It *is* in the game — in the **executable**, not the CPK.

## Where the data lives

`main` (the game NSO, ExeFS) is a **compressed** NSO: three LZ4-compressed
segments (`.text`, `.rodata`, `.data`). Each segment is a *single* LZ4 block
(`LZ4_decompress_safe(src, dst, compressed_sizes[seg], decomp_size)`; the LZ4
length-extension terminator is `255`). The `.rela.dyn` table (`R_AARCH64_RELATIVE`,
98211 entries) resolves global pointers.

Inside, there is a **per-CharaSet record table** indexed by the `CHARASET` enum:

```
base   0x2741bb8   (in the decompressed .data segment)
stride 0x48        (9 × u64)
idx    = CHARASET enum value (1-based; see Debug/_UcEnum_CharaSet.inc)
```

Record fields (u64 word indices):

| idx | meaning |
|-----|---------|
| 0   | pointer → Japanese label |
| 1   | packed `{u8 class_id, u8 gender(1=M,2=F), u8 variant(0..8), u8 _}` |
| 2,3 | other (CoA preset, model ids) |
| 6   | pointer → `CharaImg<ABBR>` name |
| 7,8 | flags |

So the engine's appearance key is exactly **`(class_id, gender, variant)`**,
where `class_id` is the `CLASSTYPE` index (`Debug/_UcEnum_Class.inc`).

The scripts reach it by name: `DrCreateChara(objectName, CharaDefName, CoAPreset)`
(the string overload, `FUN_0031b0dc`) resolves `CharaDefName` → CharaSet via
`FUN_0011a100`, then calls `FUN_00175be0`. There is also an int overload
`DrCreateChara(objectName, int CharaSet, CoAPreset)`.

## From `(class, gender, variant)` to the mask

Within a class MBS the layers that select an appearance all share a common
attribute group. Empirically:

```
mask = group(class) | (1 << variant)
```

`group(class)` is read from the class MBS `s4.attributes`: split each nonzero
attribute into `group = a & ~0xFF` and `low = a & 0xFF`; the group is the one
with the most distinct `low` values (ties → smallest group).

Verified against the previously hand-written rows, which it reproduces exactly:

| class | group | Generic | Primm (v1) | Sharon (v2) |
|-------|-------|---------|------------|-------------|
| CLERIC_F | `0x10000` | `0x10001` | `0x10002` | `0x10004` |
| WITCH_F  | `0x8000`  | `0x8001`  | —          | Yahna (v1) `0x8002` |

Rendering `--class CLERIC_F` with the generated table is **byte-identical** to
the old handwritten table.

## Regenerating

1. Decompress the game NSO (`ExeFS/main`) into `.text`/`.rodata`/`.data`.
2. Parse `.rela.dyn` (`DT_RELA` points at `.rodata+0x48`: 98211 × 24-byte
   entries) to relocate pointers.
3. Read the CharaSet records at `0x2741bb8` (stride `0x48`).
4. Map `class_id` → class name (`Debug/_UcEnum_Class.inc`) and `gender` →
   `M`/`F`; the viewer key is `<CLASS>_<M|F>`.
5. Dump each class MBS `s4` with the extractor shell
   (`printf 'd Chara/<Class>.mbs --section s4\nq\n' | ./build/rosalinde --cpk ~/Unicorn.CPK`) and
   compute `group(class)` as above.
6. Emit `viewer/data/variants.tsv`:
   `class_key <TAB> CharaSetName <TAB> 0x<group|1<<variant> <TAB> jp_label`.

The raw exe table (all 1388 CharaSets with class/gender/variant) is checked in
as `viewer/data/charaset.tsv` for reference.

## Caveats

- `group(class)` is a heuristic derived from the MBS alone. It is exact for
  single-group classes (Cleric, Witch, Swordsman, …); classes whose characters
  combine several attribute groups (e.g. the princesses) may still be
  approximations until the engine's exact per-layer test is reversed.
- `classes.tsv` is now extracted from the executable (`tools/dump_chara_table.py`),
  so the class → asset mapping is exact. Only `variants.tsv` (the per-class
  appearance masks) is still produced by the older heuristic process.
- A few CharaSets pick an asset slot the class leaves empty (e.g. `TEST_E53`
  is `WEREFOX` gender `M`, but only `Chara/WereFox_F.mbs` exists); the engine
  falls back to the filled slot — an asset-resolver concern, not a table one.
