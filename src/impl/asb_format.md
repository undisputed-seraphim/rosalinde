# ASB — AngelScript Compiled Binary (Vanillaware variant)

## Overview

`.asb` files are compiled AngelScript bytecode containers. Vanillaware uses a
**custom serialization format** — not the standard `asIScriptModule::SaveByteCode`
output. The opcode numbering matches the AngelScript 2.x dispatch table, but the
container layout and instruction packing differ.

Source files are `.ascp` (AngelScript Compiled Preprocessed?), compiled into `.asb`.

## Container format

```
[ 8-byte header  ]
[ function blocks... ]
[ embedded source path (C:/Project2/Unicorn/Soft/Data_JP/Script/*.ascp) ]
[ post-source data (enum tables, additional functions) ]
```

### 8-byte header

```
offset 0-3: u32 LE  module flags (always 0 in observed files)
offset 4-7: u32 LE  unknown (values: 0x6601, 0x6602)
```

### Function blocks (pre-source)

Each function starts with a marker byte followed by a null-terminated name:

```
[ marker: 0x08 or 0x0a ]
[ function_name\0 ]
[ bytecode ... ]
```

The next function block or the source path immediately follows the bytecode.
There is NO length prefix — the boundary is found by scanning for the next
marker byte (`0x08`/`0x0a`) followed by an alpha character leading to a valid
identifier name.

Common marker values:
- `0x08` — standard function (MAIN, BATTLE_MAIN, FIELD_MAIN, IDLE, etc.)
- `0x0a` — entry-point function (START)

### Source path

A null-terminated string embedded between the pre-source and post-source
sections:

```
C:/Project2/Unicorn/Soft/Data_JP/Script/<filename>.ascp
```

The path always starts with "C:/" and ends with ".ascp\0".

### Post-source data

After the source path, the format changes:
- Multi-byte preamble bytes precede function markers (`10 00 04 00 01 72 ...`)
- Function names may use length-prefixed encoding (not null-terminated)
- Contains enum constant tables, global variable initializers, and additional functions

This section is not fully decoded by the parser.

## Bytecode format

### Packing

**Critical difference from standard AngelScript**: Instructions are packed tightly
with no dword alignment. Standard AngelScript stores each instruction as a single
4-byte dword (opcode byte + zero pad byte + arg word). The ASB format stores only
the effective bytes.

| Instruction | AngelScript (dwords) | ASB (packed bytes) |
|------------|---------------------|-------------------|
| RET (0x3D, NO_ARG) | 4 | 1 |
| SUSPEND (0x3F, NO_ARG) | 4 | 1 |
| PshC4 (0x02, wW_ARG) | 4 | 3 (op + u16) |
| CALLSYS (0x3D, DW_ARG) | 8 | 5 (op + u8 + u32) |
| ALLOC (0x40, DW_ARG) | 8 | 5 (op + u32) |
| LDV (0x61, wW_QW_ARG) | 12 | 9 (op + u16 + u64?) |

### Opcode table

Opcode byte values match standard AngelScript 2.x. See `asb.hpp:bc_names[]`
for the full list (199 recognized opcodes, indexed by byte value).

Key opcodes:

| Byte | Name | Args | Purpose |
|------|------|------|---------|
| 0x09 | CALL | u32 funcID | Call script function |
| 0x0A | RET | none | Return from function |
| 0x0B | JMP | i32 offset | Unconditional jump |
| 0x3C | STR | ... | Store value |
| 0x3D | CALLSYS | u8 pop, u32 funcID | Call engine-registered function |
| 0x3E | CALLBND | u8 pop, u32 funcID | Call bound function |
| 0x3F | SUSPEND | none | Yield to engine |
| 0x40 | ALLOC | u32 bytes | Allocate stack space |
| 0x61 | LDV | u16 offset | Load variable by stack offset |

### CALLSYS function IDs

CALLSYS (`0x3D`) calls engine-registered functions identified by a u32 ID.
These IDs are large values (e.g. 0x55555F55, 0x5F444346) — likely CRC32
hashes of function names. The mapping from ID to function name lives in the
game binary, not in the ASB file.

## File inventory

680 ASB files in the CPK archive:

| Pattern | Count | Purpose |
|---------|-------|---------|
| `Script/Dr_OW_*` | ~430 | Overworld dialogue/drama scenes |
| `Script/FT_*` | 248 | Per-character-pair battle scripts |
| `Script/Fl_*` | 2 | Global flow scripts (Fl_OW.asb: 2.8MB) |
| `Script/CallCreateDrama.asb` | 1 | Helper for creating drama instances |
| `Script/CaptureCharacter.asb` | 1 | Character capture utility |

## Example: FT_ALN_ADL.asb (Alain vs Adel battle)

```
Header: 00 40 94 20 55 43 5f 4c
Source: C:/Project2/Unicorn/Soft/Data_JP/Script/FT_ALN_ADL.asb
Functions: 173 (including false positives from constant data)
Key functions: IDLE, WALK, START, BASE, SKILL, DRAW, FIRE, LINE (89KB)
```

The `LINE` function (89,511 bytes) is the main battle flow. Smaller functions
handle specific subsystems: SKILL (skill logic), DRAW (rendering), FIRE (effects).

## Relationship to MBS animation

ASB scripts do NOT directly reference MBS track names. The animation state
machine lives in the MBS data (section_9/section_a) — not in ASB. ASB scripts
contain game logic (battle rules, unit placement, dialogue triggers). Animation
sequencing is external to ASB.

## Parser implementation

`src/impl/asb.hpp` + `src/impl/asb.cpp` — container parser + opcode table.
`tools/extractor.cpp` — `--asb-dump` flag for CLI inspection.

The parser extracts: header, function names, bytecode, source path.
It does NOT decode: string constants, variable declarations, call targets.

## Unknowns

- Post-source function name encoding (length-prefixed vs descriptor-based)
- Header bytes 4-7 meaning (0x6601 vs 0x6602)
- CALLSYS ID → function name mapping (requires game binary)
- Full instruction size table for all 199 opcodes (only ~40 are known)
