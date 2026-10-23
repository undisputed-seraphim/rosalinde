# asbrun — static ASB inspector

`asbrun` is a CPK-backed interactive shell for inspecting compiled `.asb`
modules. It decodes bytecode, dumps functions and static call scenes, scans the
archive, and generates runtime tables. It does **not** execute scenes.

Scene execution lives in the viewer's headless mode, which supplies the MBS
track lengths/names and the opening prelude that asbrun never had:

```bash
env -u DISPLAY ./build/viewer/viewer --cpk ~/Unicorn.CPK \
  --script Script/Dr_OW_0_Opening.asb --headless --run-to 480 \
  --state - --trace /tmp/scene.ndjson
```

The old `play`, `trace`, `record`, `calls`, and `state` commands print a
migration hint and exit with status 2.

## Build and start

```bash
cmake -B build -S .
cmake --build build -j 8
./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

If stdin is not a TTY, `asbrun` runs in script mode:

```bash
printf 'open Dr_OW_0_Opening.asb\nsummary\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

## Loading a script

```text
open Script/Dr_OW_0_Opening.asb
open Dr_OW_0_Opening.asb
```

`open` accepts only `.asb` entries. If the path has no extension, `.asb` is
tried automatically. If the path has no directory and the filename is unique in
the archive, that file is used. Non-ASB entries are rejected before decoding.

```text
close          unload the current module
status         show the CPK and current module
```

Opening a new module replaces the current one. If parsing fails, the previous
module remains loaded.

## Commands

| Command | Purpose |
|---|---|
| `summary [--json FILE\|-] [--compact]` | module size, parse status, function/native/asset/section counts |
| `scan [...]` | bulk scan all `Script/*.asb` entries |
| `export chara\|enums\|bgs [--out FILE\|-]` | generate table TSVs from the CPK |
| `funcs` | list module functions |
| `disasm [FUNC] [--all] [--dir DIR]` | disassemble one function, everything, or write a text dump |
| `scene [FUNC] [--json FILE\|-] [--ir] [--index] [--limit N] [--filter TEXT] [--chara-file FILE]` | extracted call stream, SceneIR, or compact scene index |
| `timeline FUNC [--expand-calls] [--json FILE\|-] [--limit N]` | static frame-annotated call timeline |
| `exec [FUNC]` | run a VM function without scene modeling |
| `eval SOURCE` | compile and run AngelScript source |
| `source FILE` | run commands from a file |
| `help [COMMAND]` | command help |
| `quit`, `exit`, `q` | leave the shell |

`FUNC` is optional for `exec`; the default search order is `scene00`, then
`MAIN`.

## Static inspection

```text
summary
funcs
disasm scene00
scene --index
scene scene00 --filter DrCreate
scene --ir --json /tmp/scene-ir.json
timeline scene00 --expand-calls
```

- `scene` text prints one line per event; the default limit is 50 events.
- `scene --limit N` caps output events; `--limit 0` means unlimited.
- `scene --filter TEXT` keeps calls whose name, declaration, or string arguments
  contain `TEXT`.
- `scene --ir` lists folded entities; `--chara-file FILE` overrides the default
  `viewer/data/generated/chara_table.tsv` resolver.
- `scene --index` prints **static event counts**, not runtime entities:
  sprite/chara/entity/stage/camera calls.
- `timeline` walks the extracted event stream and accumulates static `Wait(n)`
  frames. It is linear and branch-unaware.

### Bulk scan and table export

```text
scan --limit 5
scan --summary --json /tmp/all-summary.json
scan --scene-index --json -
scan --scene-ir --json /tmp/all-ir.json --chara-file viewer/data/generated/chara_table.tsv
scan --disasm-dir /tmp/disasm-all
```

- `scan` iterates `Script/*.asb` and defaults to a concise text summary.
- `--summary`, `--scene`, `--scene-index`, and `--scene-ir` select the JSON
  shape; `--json FILE|-` is required for those structured modes.
- `--filter TEXT` filters by CPK path; `--limit N` caps modules.
- `--disasm` writes disassembly to stdout; `--disasm-dir DIR` writes one file
  per module.

Table generation:

```text
export chara --out viewer/data/generated/chara_table.tsv
export enums --out viewer/data/generated/uc_enums.tsv
export bgs   --out viewer/data/generated/battlebgs.tsv
```

## VM-only execution

`exec` and `eval` still use the AngelScript host, but deliberately without the
scene registry, Wait scheduler, prelude, or MBS providers. They are useful for
loading/compile/exception checks and for small language experiments; they do
not produce a scene and should not be used to reason about drama timing.

```text
exec scene00
eval "int main(){ return 0; }"
```

## Scene execution moved to the viewer

Use the viewer's headless mode for frame-accurate execution:

```bash
env -u DISPLAY ./build/viewer/viewer --cpk ~/Unicorn.CPK \
  --script Script/Dr_OW_0_Opening.asb --headless --run-to 2600 \
  --trace /tmp/trace.ndjson \
  --record /tmp/record.ndjson.gz \
  --calls /tmp/calls.txt --calls-json /tmp/calls.json \
  --state - --state-json /tmp/state.json
```

- `--run-to N` stops at a scene frame; `--scene-frame N` is the fallback.
- `--trace FILE|-` writes NDJSON; categories include `vm`, `stage`, `camera`,
  `asset`, `sprite`, `draw`, and `timeline`.
- `--record FILE` writes full scene-state NDJSON per `--record-every` frame;
  a `.gz` suffix compresses it.
- `--calls`/`--calls-json` write the native-call census.
- `--state`/`--state-json` snapshot the scene, including selected/resolved MBS
  tracks, run counts, drawability, and missing-asset reasons.
- `--probe DIR` writes state/summary (and draw dumps only when rendering).
- `--scene-func FUNC` selects any module function, not only the auto-picked
  scene entry.

The viewer installs MBS track lengths and name resolution, so
`WaitSpriteMoneAnimationEnd`, `DrAnimationWait`, `DrTurn`, and camera/move waits
carry real time. `asbrun` intentionally does not.

## Output conventions

- Diagnostics, load messages, and command status go to **stderr**.
- Command results, JSON, and NDJSON go to **stdout** unless a file path is given.
- In script mode, command failures set the process exit status to `1`; the shell
  continues running the remaining commands, like `rosalinde`.
- Retired scene-execution commands return `2` with a viewer hint.

## Known limitations

- `scene`, `timeline`, and `scan --scene*` reuse the linear static extractor and
  are branch-unaware.
- `exec`/`eval` do not model the scene, waits, or MBS providers.
- `asbrun` no longer loads runtime tables for execution; `export` and `scene --ir`
  still read the generated tables from the data directory.
