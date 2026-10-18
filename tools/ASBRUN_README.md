# asbrun — interactive ASB shell

`asbrun` is a CPK-backed interactive shell for inspecting and running AngelScript
bytecode (`.asb`). It is the interactive counterpart to `rosalinde`: open the CPK
once, load a script by path or filename, then inspect or run it without restarting
the process.

The existing `asb_harvest`, `asb_run`, `asb_trace`, and `asb_record` binaries are
still built and remain the reference implementations. `asbrun` is the new unified
frontend under validation.

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
| `funcs` | list module functions |
| `disasm [FUNC] [--all] [--dir DIR]` | disassemble one function, everything, or write a text dump |
| `scene [FUNC] [--json FILE\|-] [--ir] [--index] [--limit N] [--filter TEXT]` | extracted call stream, SceneIR, or compact scene index |
| `play [FUNC] ...` | run a function with the scene scheduler |
| `trace ...` | alias for `play` with an NDJSON trace |
| `record ...` | alias for `play` with a timeline JSON |
| `exec [FUNC]` | execute a function immediately, without the scheduler |
| `eval SOURCE` | compile and run AngelScript source |
| `source FILE` | run commands from a file |
| `help [COMMAND]` | command help |
| `quit`, `exit`, `q` | leave the shell |

`FUNC` is optional for `play`, `trace`, `record`, and `exec`; the default search
order is `scene00`, then `MAIN`.

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
- `scene --filter TEXT` keeps calls whose name or declaration contains `TEXT`.
- `scene --ir` lists folded entities.
- `scene --index` prints **static event counts**, not runtime entities created by
  a given frame:
  - `sprite calls`: `CreateSprite*` calls
  - `chara calls`: `DrCreate*Chara` / `DrCreateNpc` calls
  - `entity calls`: sprite + chara
  - `stage calls`: stage/unit/symbol setup calls
  - `camera calls`: camera position/parameter calls

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
--data-dir DIR          runtime data directory (default: viewer/data)
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

Run a command file:

```bash
./build/tools/asb/asbrun --cpk ~/Unicorn.CPK <<'EOF'
open Dr_OW_0_Opening.asb
summary
scene --index
q
EOF
```

## Deferred features

The following are not in the initial `asbrun` validation build:

- bulk `scan` and `export` commands (`--all`, enum/BGS/chara generation)
- static frame-annotated `timeline`
- complete native-call trace and call census
- viewer state snapshot

The old ASB binaries remain available for those workflows until the new shell is
validated.
