# asbrun — interactive ASB shell

`asbrun` is a CPK-backed interactive shell for inspecting and running AngelScript
bytecode (`.asb`). It is the interactive counterpart to `rosalinde`: open the CPK
once, load a script by path or filename, then inspect or run it without restarting
the process.

`asbrun` is the unified ASB frontend. The standalone `asb_harvest`, `asb_run`,
`asb_trace`, and `asb_record` binaries have been retired.

## Build and start

```bash
cmake -B build -S .
cmake --build build -j 8
```

```bash
./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

`--cpk` is required. If stdin is not a TTY, `asbrun` runs in script mode:

```bash
printf 'open Dr_OW_0_Opening.asb\nsummary\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

## Loading a script

```text
open Script/Dr_OW_0_Opening.asb
open Dr_OW_0_Opening.asb
```

`open` accepts only `.asb` entries. If the path has no extension, `.asb` is tried
automatically. If the path has no directory and the filename is unique in the
archive, that file is used. Ambiguous filenames are reported with the matching
paths. Non-ASB entries such as `.mbs`/`.ftx` are rejected before decoding.

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
| `play [FUNC] ...` | run a function with the scene scheduler |
| `trace ...` | alias for `play` with an NDJSON trace |
| `record ...` | alias for `play` with a timeline JSON |
| `calls ...` | run and report native-call counts, stubs, and uncalled imports |
| `state [FUNC] [--frame N] [--json FILE\|-]` | snapshot runtime entities and resolved MBS tracks |
| `exec [FUNC]` | execute a function immediately, without the scheduler |
| `eval SOURCE` | compile and run AngelScript source |
| `source FILE` | run commands from a file |
| `help [COMMAND]` | command help |
| `quit`, `exit`, `q` | leave the shell |

`FUNC` is optional for `play`, `trace`, `record`, `calls`, `state`, and `exec`;
the default search order is `scene00`, then `MAIN`.

## Summary and scene inspection

```text
asbrun:Script/Dr_OW_0_Opening.asb> summary
```

JSON stays compatible with the corresponding `asb_harvest` modes:

```text
summary --json /tmp/summary.json
summary --json - --compact
scene --json /tmp/scene.json
scene --index --json /tmp/index.json
scene --ir --json /tmp/ir.json
```

- `--json FILE` writes the document to a file.
- `--json -` writes it to stdout.
- `summary --compact` writes counts only instead of the full string/call/API pools.
- `scene` text prints one line per event; default text limit is 50 events.
- `scene --limit N` caps output events; `--limit 0` means unlimited.
- `scene --filter TEXT` keeps calls whose name, declaration, or string arguments
  contain `TEXT`.
- `scene --ir` lists folded entities; `--chara-file FILE` overrides the default
  `viewer/data/generated/chara_table.tsv` resolver.
- `scene --index` prints **static event counts**, not runtime entities created by
  a given frame:
  - `sprite calls`: `CreateSprite*` calls
  - `chara calls`: `DrCreate*Chara` / `DrCreateNpc` calls
  - `entity calls`: sprite + chara
  - `stage calls`: stage/unit/symbol setup calls
  - `camera calls`: camera position/parameter calls
  - JSON also retains the legacy `sprites`, `chara`, `stages`, and `cameras`
    fields for `asb_harvest` compatibility.

## Bulk scan and table export

```text
scan --limit 5
scan --summary --json /tmp/all-summary.json
scan --scene-index --json -
scan --scene-ir --json /tmp/all-ir.json --chara-file viewer/data/generated/chara_table.tsv
scan --disasm-dir /tmp/disasm-all
```

- `scan` iterates `Script/*.asb` and defaults to a concise text summary.
- `--summary`, `--scene`, `--scene-index`, and `--scene-ir` select the JSON object
  shape; `--json FILE|-` is required for those structured modes.
- `--filter TEXT` filters by CPK path.
- `--limit N` caps the number of modules.
- `--disasm` writes disassembly to stdout; `--disasm-dir DIR` writes one file per
  module.
- `scan --json` streams the array; it does not build the old ~176 MB string.

Table generation:

```text
export chara --out viewer/data/generated/chara_table.tsv
export enums --out viewer/data/generated/uc_enums.tsv
export bgs   --out viewer/data/generated/battlebgs.tsv
```

`export chara` scans all modules; `export enums` reads `Debug/_UcEnum_*.inc`;
`export bgs` scans `BattleBG/*`. The generated rows match `asb_harvest`; only the
generated header text identifies `asbrun`.

Known limitations:

- `scan --scene`, `--scene-index`, and `--scene-ir` reuse the linear static
  extractor and are branch-unaware, like the single-module scene commands.
- `scan --json -` can produce very large output; prefer `--json FILE`.

## Static timeline

```text
timeline scene00 --expand-calls
timeline scene00 --json /tmp/timeline.json
```

The timeline walks the extracted event stream and accumulates `Wait(n)`. Each row
contains `frame`, formatted call, source kind, and (for imports) the source module.
`--expand-calls` expands local module calls one level, so their waits count inside
the caller's frame position.

Known limitation: the static lift is linear and branch-unaware. It is accurate for
the opening's early `Wait` sequence but can diverge where the script branches,
loops, or computes arguments dynamically.

## Running a script

`play` uses `asb::RuntimeSession`, so `Wait(n)` parks the script and advances the
scene clock:

```text
play scene00 --frames 2600
```

Options:

```text
--frames N              maximum scene frames (default 200000)
--trace FILE|-          write NDJSON trace events
--record FILE|-         write recorded timeline JSON
--categories LIST       trace categories, e.g. vm,stage,camera,sprite
--chara-table FILE      override the character display-name table
--data-dir DIR          runtime data directory (default: viewer/data/generated or installed share/rosalinde)
```

`play` can write trace and record output from the same run:

```text
play scene00 --frames 2600 \
  --trace /tmp/opening.ndjson \
  --record /tmp/opening.json
```

### `trace` alias

`trace` is `play` with trace output enabled. It defaults to stdout:

```text
trace scene00 --frames 2000 --categories vm
trace scene00 --frames 2000 --categories vm --out /tmp/vm.ndjson
```

`vm/call` events are complete native calls:

```json
{"frame":0,"kind":"vm","event":"call","name":"SetSpriteMoneAnimation",
 "call":"SetSpriteMoneAnimation(\"ヴァニラロゴ\", 9, 0, 0)",
 "caller":"scene00","handled":true}
```

Non-void calls may also carry `return_type` and `return`. The retired `asb_trace`
implementation emitted name-only calls; `asbrun` emits the complete event.
Because dispatch must run before `handled`/return are known, the complete
`vm/call` event is emitted after any semantic events from the same call.

### `calls` command

`calls` runs a scene and reports native-call counts without writing a trace:

```text
calls scene00 --frames 2000
calls scene00 --frames 2000 --json /tmp/calls.json
```

Text output includes calls, stub calls, first/last frame, never-called imported
natives, and called-but-unhandled natives. `--json -` writes the machine-readable
form to stdout.

### `state` command

`state` snapshots the live runtime scene at a frame:

```text
state scene00 --frame 480
state scene00 --frame 480 --json /tmp/state-480.json
```

It reports one entry per runtime entity, including the runtime fields and the
resolved MBS track name/run count. `drawn` is a runtime predicate
(`started && visible && alpha > 0 && resolved track`); it is not a renderer probe
and does not include asset-load failures or actual GPU draw bounds.

Known limitation: `state` is a CLI/runtime snapshot. It does not supply the MBS
track-length/name callbacks, so `WaitSpriteMoneAnimationEnd`, `DrAnimationWait`, and
`DrTurn` do not carry real time here. For frame-accurate state plus renderer state,
use the viewer's `--probe` output; `asbrun` remains CLI-only.

### `record` alias

`record` is `play` with timeline recording enabled. It defaults to stdout:

```text
record scene00 --frames 1200 --json /tmp/opening.json
record scene00 --frames 20
```

The new record serializer fixes the old duplicate `"field"` key and escapes
entity/source strings consistently.

Record JSON top-level keys:

```text
source, function, keyframes, fades, stages, texts, shakes, bokashi, variations,
effects, point_lights, audio, items, callbacks, fog, gradients, ui, field
```

`keyframes` is state-change based, not one entry per scene frame. Each keyframe
contains `frame`, `camera`, and `entities`; each entity includes `name`, `kind`,
`mbs`, `ftx`, `track`, `track_index`, `alpha`, `visible`, `layer_mask`, `color`,
`order`, and `aliases`.

### Immediate execution

`exec` uses `Host::execute` and does not model the scene scheduler. `Wait(n)`
does not advance a virtual clock there:

```text
play scene00 --frames 600   # reaches the title at frame 450
exec scene00                # runs through the Wait calls immediately
```

`eval` compiles a standalone AngelScript source string:

```text
eval "int main(){ return 0; }"
```

## Output conventions

- Diagnostics, load messages, runtime registration messages, and command status
  go to **stderr**.
- Command results, JSON, and NDJSON go to **stdout** unless a file path is given.
- In script mode, command failures set the process exit status to `1`; the shell
  continues running the remaining commands, like `rosalinde`.

This means machine-readable output remains clean:

```bash
printf 'open Dr_OW_0_Opening.asb\nrecord scene00 --frames 10\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK \
  | jq .
```

## Examples

Inspect a module:

```bash
printf 'open Dr_OW_0_Opening.asb\nsummary\nfuncs\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Inspect scene events:

```bash
printf 'open Dr_OW_0_Opening.asb\nscene scene00\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Trace the opening:

```bash
printf 'open Dr_OW_0_Opening.asb\ntrace scene00 --frames 2000 --categories vm --out /tmp/vm.ndjson\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Record and trace one run:

```bash
printf 'open Dr_OW_0_Opening.asb\nplay scene00 --frames 2600 --trace /tmp/opening.ndjson --record /tmp/opening.json\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Build a static timeline and call census:

```bash
printf 'open Dr_OW_0_Opening.asb\ntimeline scene00 --expand-calls\ncalls scene00 --frames 2000\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Snapshot runtime state at a frame:

```bash
printf 'open Dr_OW_0_Opening.asb\nstate scene00 --frame 480 --json /tmp/state-480.json\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Run a command file:

```bash
./build/tools/asb/asbrun --cpk ~/Unicorn.CPK <<'EOF'
open Dr_OW_0_Opening.asb
summary
scene --index
q
EOF
```
