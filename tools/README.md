# rosalinde extractor

`rosalinde` is the CPK browser and extractor for Unicorn Overlord assets. It opens
a CPK once, builds a virtual directory tree from the TOC, and then accepts commands
on stdin. Opening the archive and listing its contents never decompresses entries.

## Build

```bash
cmake -B build -S .
cmake --build build -j 8
```

The executable is `build/rosalinde`.

## Startup

```text
rosalinde --cpk PATH
rosalinde --cpk=PATH
```

`--cpk` is the only startup flag. If stdin is a terminal, it prints a prompt and
accepts commands interactively. If stdin is a pipe or file, it runs in script mode
without a prompt.

## Script mode

Commands are line-oriented. One command per line is the stable scripting protocol.

```bash
printf 'ls\nstat OTM/BattleBG/BGBtWastelandMist.otm\nq\n' \
  | ./build/rosalinde --cpk ~/Unicorn.CPK
```

Rules:

- command output goes to stdout
- prompts and diagnostics go to stderr, so piped stdout is clean
- no prompt or banner is printed when stdin is not a terminal
- blank lines are ignored
- a line whose first non-space character is `#` is a comment
- arguments may be single- or double-quoted
- `quit`, `exit`, or `q` stops processing
- EOF is a clean exit
- in script mode, the process exits nonzero if any command failed

Use `source` to run a reusable command file:

```text
source /tmp/cpk-commands.txt
```

```bash
./build/rosalinde --cpk ~/Unicorn.CPK <<'EOF'
source /tmp/cpk-commands.txt
q
EOF
```

`source` paths are host filesystem paths, not CPK paths. Nested source files are
limited to 16 levels.

## Interactive line editing

When stdin is a terminal, the prompt uses an in-process line editor.

History:

- the last five distinct interactive commands are remembered
- repeated commands move to the front
- `Up` walks from newest to oldest
- `Down` walks back toward the newest entry
- `Down` past the newest restores the draft line
- commands from `source` files and piped stdin do not enter interactive history

Editing keys:

- `Left` / `Right`
- `Home` / `End`
- `Backspace` / `Delete`
- `Ctrl-A` / `Ctrl-E`
- `Ctrl-U` / `Ctrl-K` / `Ctrl-W`
- `Ctrl-L` redraw
- `Ctrl-C` clears the current line
- `Ctrl-D` exits on an empty line

Mouse wheel events are ignored, and mouse reporting is turned off when the editor
starts. Terminal scrollback can still be used with `Shift+PageUp` and
`Shift+PageDown`.

`PageUp` and `PageDown` are intentionally ignored. Once the editor owns raw
terminal input, the page keys cannot be forwarded to the terminal's scrollback.

Terminal settings are restored when the editor exits. Script mode does not use
the line editor and is unaffected by these bindings.

## Commands

| Command | Purpose |
|---|---|
| `ls [PATH]` | list immediate virtual directory contents |
| `ls -l [PATH]` | include IDs, compressed/decompressed sizes, and ratios |
| `find [PATH] [TEXT] [--long]` | recursively find files by case-insensitive path text |
| `stat PATH` | show file metadata or directory totals |
| `e VIRTUAL [REAL]` | extract a file or directory tree |
| `d PATH [options]` | inspect a file in memory without writing to disk |
| `help [COMMAND]` | show command help |
| `source FILE` | run commands from a host file |
| `quit`, `exit`, `q` | leave the shell |

Paths are virtual CPK paths relative to the current directory. The initial
directory is `/`. `.` and `..` are supported.

### Listing and searching

```text
rosalinde:/> ls
BattleBG/
Chara/
...
NodeNetworkData/
OTM/
Sound/
```

```text
rosalinde:/> ls -l NodeNetworkData
2248: NodeNetworkData/-10400_-6976_-8992_-5984.nnd  compressed=7332  decompressed=16452  ratio=2.24
...
```

`find` matches file paths only; it does not list directories. `--long` uses the
same metadata format as `ls -l`.

```text
find / --long
find NodeNetworkData .nnd
find OTM/BattleBG .otm --long
```

`stat` reports one file:

```text
Path: OTM/BattleBG/BGBtWastelandMist.otm
Type: file
ID: 2553
Compressed: 320
Decompressed: 336
Ratio: 1.05
Offset: 1636890624
```

or recursive totals for a directory:

```text
Path: NodeNetworkData
Type: directory
Entries: 302
Files: 302
Directories: 0
Compressed: 9290284
Decompressed: 19876288
Ratio: 2.14
```

### Extraction

```text
e VIRTUAL_PATH [REAL_PATH]
```

`REAL_PATH` defaults to `.`. It accepts `~` and `~/subdir` paths. Existing files
are overwritten.

For a file, if `REAL_PATH` is an existing directory or ends with `/`, the source
filename is appended. Otherwise `REAL_PATH` is the exact output file path.

For a directory, `REAL_PATH` is the destination directory itself. Its contents
are extracted relative to the selected virtual directory:

```text
e OTM/BattleBG /tmp/otm-bg
```

puts the four `.otm` files directly in `/tmp/otm-bg`. Re-running the same command
overwrites them without creating another nested directory.

Use `/` as the source to extract the entire archive tree:

```text
e / /tmp/unicorn
```

Each extracted file reports its compressed and decompressed size. Directory
extraction also prints a total.

### Dumping/inspection

`d` autodetects the extension and inspects the entry in memory. It does not write
files.

| Extension | Inspection |
|---|---|
| `.acb` | recursive UTF tables |
| `.acf` | recursive UTF tables |
| `.bbs` | battle background assets + objects/cameras/lights/fog/environment/materials |
| `.nnd` | node/edge/name/link graph |
| `.ftx` | texture dimensions, format, and sizes |
| `.mbs` | tracks, sequences, frames, raw sections |
| `.mpd` | field quad mesh and texture indices |
| `.otm` | audio timeline |
| `.ufb` | field header, 24-byte records, string pool |
| `.ucb` | dense field grid cells |
| `.fms` | string table |
| `.asb` | deferred |
| unknown | metadata, plus a 64-byte hex preview for files up to 1 MiB |

Common options:

```text
d PATH [--json] [--full] [--limit N]
```

- `--json` produces a machine-readable document
- `--full` expands text output
- `--limit N` caps rows/nodes/tracks/sequences/frames

MBS sequence queries also accept `--timeline` and `--colors`; both require
`--track` and `--seq`.

## MBS inspection

MBS files are treated as a three-level animation hierarchy:

```text
section 9 track
  -> section a sequence
       -> section 8 frame
```

Section 9 is the root for this view. In the full corpus every section-a row is
referenced exactly once by a track, and every section-8 row is referenced exactly
once by a sequence.

Track names are not unique. Use the numeric track index; the names are printed
for identification.

### Default MBS dump

```text
d BattleBG/BGBtPort_D.mbs
```

```text
tracks (38):
  [0] MONEANM_0 seqs=14 [0..13] bounds=(-2242.5,-676,3264.16,529.696) main=0 disabled=0
  [1] MONEANM_1 seqs=1 [0] bounds=(-1228.75,149.531,1574.75,733.938) main=0 disabled=0
```

The default is deliberately compact: one line per track, with the number of
sequences and a sequence index range. Sequence details are not expanded.

### Inspect a track

```text
d BattleBG/BGBtPort_D.mbs --track 0
```

```text
track[0] MONEANM_0 seqs=14 sa=0 bounds=(-2242.5,-676,3264.16,529.696) main=0 disabled=0
  seq[0] sa=0 s8[0..3) total_frames=40001 once=-1 commit=-1 track=0 loop=0
  seq[1] sa=1 s8[3..4) total_frames=1 once=-1 commit=-1 track=6 loop=0
  ...
```

- `seq[N]` is relative to the track
- `sa=N` is the global section-a index
- `--limit N` caps the number of sequences printed

### Inspect a sequence and its frames

```text
d BattleBG/BGBtPort_D.mbs --track 0 --seq 2
```

```text
sequence[2] sa=2 s8[4..10) total_frames=211 once=0 commit=-1 track=7 loop=0
  frame[0] s8=4 duration=42 flags=0x21 [FLIPX|ACTIVE] s6=37 s7=1 s8_loop_target=0 ...
  frame[1] s8=5 duration=42 flags=0x21 [FLIPX|ACTIVE] s6=38 s7=1 s8_loop_target=0 ...
```

- `frame[N]` is relative to the sequence
- `s8=N` is the global section-8 index
- `duration` is the section-8 `s8_duration_ticks` field
- `total_frames` is the sum over the sequence
- `--limit N` caps the number of frame rows
- sections 4, 5, 6, and 7 are not expanded; their IDs are reported

### Sequence timeline

```text
d BattleBG/BGBtPort_D.mbs --track 0 --seq 2 --timeline
```

Each frame row gains sequence-local cumulative timing:

```text
frame[0] t=0 end=42 s8=4 duration=42 ...
frame[1] t=42 end=84 s8=5 duration=42 ...
frame[4] t=168 end=210 s8=8 duration=42 ...
frame[5] t=210 end=211 s8=9 duration=1 ...
```

- `t` is the sum of preceding section-8 durations in the sequence
- `end = t + duration`
- JSON frame objects always include `t` and `end`, even without `--timeline`
- the sequence header includes `s8_start_offset`; the inspector shows raw section-8 rows

### Effective color/alpha

```text
d GUI/Logo.mbs --track 9 --seq 21 --colors
```

For each section-8 frame, each layer in its section-6 keyframe is shown with:

- section-4 and section-0 indices
- attributes, flags, blend, and texture IDs
- raw `s0` corner colors
- corner colors after multiplication by `s7.fog`
- effective colors as `#RRGGBBAA`

```text
frame[0] ... fog=0xffffffff
  layer[0] s4=344 s0=16 attr=0x0 flags=0x1 blend=0 tex=0 raw=[...] eff=[#ffffffff,...]
```

`--colors` works with or without `--timeline`, and with `--json`. JSON layers
contain `raw_colors` and effective `colors` arrays.

This is endpoint inspection, not interpolation: it shows the source keyframe's
layer colors and the fog multiplier.

### Full JSON

The full MBS JSON dump is unchanged and remains the complete machine-readable
document:

```text
d BattleBG/BGBtPort_D.mbs --json
```

It contains header metadata and all 12 raw sections:

```json
{
  "type": "mbs",
  "version": 119,
  "section_counts": { "s4": 239, "s9": 38 },
  "sections": {
    "s4": {
      "columns": ["s4_source_tile_id", "color_id", "flags"],
      "rows": [[0, 0, 1]]
    }
  }
}
```

### Filtered JSON

```text
d BattleBG/BGBtPort_D.mbs --track 0 --json
d BattleBG/BGBtPort_D.mbs --track 0 --seq 2 --json
```

Filtered JSON adds `track`, `sequences`, or `sequence` and `frames` objects. It
keeps both relative and global indices:

```json
{
  "track": {
    "index": 0,
    "name": "MONEANM_0",
    "sa_index": 0,
    "sequence_count": 14
  },
  "sequence": {
    "sequence_index": 2,
    "sa_index": 2,
    "s8_index": 4,
    "s8_count": 6,
    "total_frames": 211
  },
  "frames": [
    {
      "frame_index": 0,
      "s8_index": 4,
      "duration": 42,
      "flags": 33,
      "s6_keyframe_id": 37,
      "s7_transform_id": 1
    }
  ]
}
```

If `--limit` truncates a list, the JSON includes `sequences_truncated` or
`frames_truncated`.

### Raw sections

```text
d BattleBG/BGBtPort_D.mbs --full
d BattleBG/BGBtPort_D.mbs --section s4
d BattleBG/BGBtPort_D.mbs --section s4 --limit 20
```

`--full` prints all 12 raw section tables. `--section` selects one section
(`s0` through `s9`, `sa`, `sb`). Raw section modes omit the compact track list.
These raw modes cannot be combined with `--track` or `--seq`.

## Example MBS script

Create `/tmp/mbs-commands.txt`:

```text
# list an MBS track and a sequence's frames
ls BattleBG
d BattleBG/BGBtPort_D.mbs
d BattleBG/BGBtPort_D.mbs --track 0
d BattleBG/BGBtPort_D.mbs --track 0 --seq 2
d BattleBG/BGBtPort_D.mbs --track 0 --seq 2 --json
```

Run it with:

```bash
./build/rosalinde --cpk ~/Unicorn.CPK <<'EOF'
source /tmp/mbs-commands.txt
q
EOF
```
