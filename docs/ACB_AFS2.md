# ACB + AFS2 File Format

## Overview

An ACB (CRI ADX2 Cue Binary) file is an audio asset container used by CRIWARE middleware.
It's a top-level `@UTF` table containing embedded sub-tables and one or more embedded AFS2 archives.
The sub-tables link cues (logical audio events) to waveforms (actual audio data) stored in AFS2 archives.

## Byte order

- **ACB root table + all sub-tables**: `@UTF` format — **big-endian** multibyte values
- **AFS2 headers + data**: **little-endian** multibyte values

This endianness split is confirmed by cross-referencing libcgss and by hex analysis:
interpreting AFS2 fields as big-endian produces nonsensical values (e.g. `file_count = 16777216`
instead of 1).

## File structure

```
ACB File (.acb)
│
├── @UTF Root Table (1 row, ~96 columns)
│   ├── Version (u32)         format version (typically 0x01420000)
│   ├── CueTable (data_t)     → embedded @UTF sub-table
│   ├── CueNameTable (data_t) → embedded @UTF sub-table
│   ├── WaveformTable (data_t)→ embedded @UTF sub-table
│   ├── SynthTable (data_t)   → embedded @UTF sub-table
│   ├── TrackTable (data_t)   → embedded @UTF sub-table (optional)
│   ├── SequenceTable (data_t)→ embedded @UTF sub-table (optional)
│   ├── AwbFile (data_t)      → embedded AFS2 archive (internal audio)
│   ├── StreamAwbAfs2Header (data_t) → embedded @UTF table (external AWB metadata)
│   └── (many other columns: AcbGuid, AcbVolume, AcfReferenceTable, etc.)
│
├── [sub-table data regions]
│   ├── CueTable rows
│   ├── CueNameTable rows
│   ├── WaveformTable rows
│   ├── SynthTable rows
│   ├── TrackTable rows
│   └── SequenceTable rows
│
└── [AFS2 data region] (when AwbFile is present)
    ├── AFS2 header (16 bytes)
    ├── Entry ID array
    ├── Entry offset array
    └── Raw audio data (HCA, ADX, etc.)
```

## Cue → Waveform resolution chain

```
cue_id (e.g. 1700000)
  ↓
CueTable → { CueId, ReferenceType, ReferenceIndex }
  ↓                                    (usually 3)
SynthTable[ReferenceIndex].ReferenceItems → binary blob → waveformIndex (u16 BE)
  ↓
WaveformTable[waveformIndex] → { EncodeType, Streaming, Id/MemoryAwbId/StreamAwbId, ... }
  ↓
if streaming == false:
    Internal AFS2 (AwbFile) → lookup by waveformId → file offset + size
    → raw audio bytes within the .acb file
if streaming == true:
    External .awb file on disk → AFS2 header from StreamAwbAfs2Header
    → lookup by waveformId → file offset + size in companion .awb
```

## Sub-table schemas

### CueTable

```
CueId            u32   Unique cue identifier
ReferenceType    u8★   How to parse ReferenceItems: 2=per-row, 3/8=shared blob
ReferenceIndex   u16   Row index into SynthTable
Length           u32   Duration in samples
NumRelatedWaveforms u16
UserData         str   (optional)
```

★ `ReferenceType` is often a **has_default** column — one value applies to all cues.
Our typed `table<>` schema handles this: check if `cue_rt.size() == 1 && cue_ids.size() > 1`
and use `cue_rt[0]` for all rows.

### WaveformTable

```
EncodeType       u8    Audio format (see below)
Streaming        u8    0 = internal AFS2, 1 = external .awb
LoopFlag         u8
NumChannels      u8
SamplingRate     u32   Hz
NumSamples       u32
MemoryAwbId      u16   Internal AFS2 file ID (when streaming=0)
StreamAwbId      u16   External AWB file ID (when streaming=1)
StreamAwbPortNo  u16
ChConfig         u32
ExtensionData    u16
LipMorthIndex    u16
```

### SynthTable

```
ReferenceItems   data  Raw binary blob containing waveformIndex (u16 BE)
                       Per libcgss: offset correction depends on ReferenceType
Type             u8
CommandIndex     u16
ControlWorkArea1 u16
ControlWorkArea2 u16
```

### ReferenceItems parsing logic

The `ReferenceItems` binary blob links cues/tracks to waveform indices.
The blob offset is relative to the SynthTable sub-table start.

For ReferenceType = **2** (per-row):
```
ref_correction = ref_size + 2
waveformIndex = read_u16BE(stBase + ref_offset + ref_correction)
```

For ReferenceType = **3** or **8** (shared, cumulative offset):
```
if first cue:
    ref_correction = ref_size - 2
else:
    ref_correction += 4
waveformIndex = read_u16BE(stBase + ref_offset + ref_correction)
```

`stBase` = the SynthTable's absolute offset within the ACB file buffer.

### CueNameTable

```
CueIndex   u16   Index into _cues array
CueName    str   Human-readable name (often just the cue ID as a string)
```

## Encode type constants

| Value | Format | Extension |
|-------|--------|-----------|
| 0     | ADX    | .adx      |
| 2     | HCA    | .hca      |
| 6     | HCA2   | .hca      |
| 7     | VAG    | .vag      |
| 8     | ATRAC3 | .at3      |
| 9     | BCWAV  | .bcwav    |
| 13    | DSP    | .dsp      |

Unicorn Overlord primarily uses HCA (encode_type=2).

## AFS2 binary format

### Header (16 bytes, all little-endian)

```
Offset  Size  Type    Field
0x00    4     char[4] Magic "AFS2"
0x04    4     u32 LE  Version
                         offset_field_size = (version >> 8) & 0xFF
                         id_field_size     = version & 0xFF  (should be 2)
0x08    4     i32 LE  File count
0x0C    4     u32 LE  Byte alignment
                         lower 16 bits = alignment (typically 0x20 = 32)
                         upper 16 bits = hca_key_modifier
```

### Entry array (file_count × offset_field_size bytes)

Each entry is `offset_field_size` bytes (typically 4), containing a u16 LE ID
at the start (remaining bytes are zero-padding).

```
Offset = 0x10 + i × offset_field_size
  u16 LE at offset+0: file ID (waveform ID)
  (remaining bytes: zero padding)
```

### Offset array (file_count × offset_field_size bytes)

Each offset is `offset_field_size` bytes, stored as a little-endian integer
masked to `offset_field_size` bytes.

```
Offset = 0x10 + file_count × offset_field_size + i × offset_field_size
  LE integer: raw byte position within the source stream
```

The raw offset is relative to the start of the AFS2 archive within the
containing stream. Add the AFS2 base offset to get the absolute position.

### Alignment and size

```
aligned_offset = round_up(raw_offset, alignment)
file_size = next_entry.offset - this_entry.offset  (except last entry)
last_entry.size = archive_end - last_entry.offset
```

Where `archive_end = archive_data_size + archive_base_offset`.

### Key detail: entry stride

The stride for both the ID array and the offset array is `offset_field_size`
(not 2 for IDs, as libcgss assumes). This was discovered via hex analysis on
Unicorn Overlord ACB files, where IDs are stored as 4-byte entries with the
actual u16 ID in the lower 16 bits and zeros in the upper 16 bits.

libcgss uses `fileOffsetFieldBase = 0x10 + fileCount * 2` (2-byte ID entries),
but the actual files use `0x10 + fileCount * offset_field_size`.
Our implementation follows the on-disk format.

## HCA key modifier

For ACB files with `format_version >= 0x01300000`, the AFS2 header's
`hca_key_modifier` (upper 16 bits of the byte_alignment field) must be
combined with the HCA cipher keys during audio decoding.

## Internal vs External AWB

### Internal AWB (AwbFile)

The `AwbFile` data_t in the root table points to an embedded AFS2 archive
within the .acb file. The audio data is stored directly in the .acb.
All offsets are absolute within the .acb file buffre.

Example: voice_add_jp.acb has AwbFile at offset 4288, size 1151 bytes,
containing 1 HCA waveform.

### External AWB (StreamAwbAfs2Header)

The `StreamAwbAfs2Header` field is itself a small `@UTF` table with one column:
`Header` (data_t) pointing to an AFS2 header blob. This AFS2 header describes
the file layout of a companion `.awb` file on disk, which contains the actual
streaming audio data.

The external .awb file is located by trying three name patterns:
1. `{base}_streamfiles.awb`
2. `{base}.awb`
3. `{base}_STR.awb`

Example: bgm.acb has both internal AFS2 (296KB, 28 files) and external AWB
header (130 files). The cue's `streaming` flag in WaveformTable determines
which archive to use.

## Internal AWB always has format version with id_field_size=2

## ACB parse flow (reference implementation)

```
ACB::parse(span<const uint8_t> data):
  1. Parse root @UTF table from data
  2. Extract Version, AwbFile from root columns
  3. Parse CueTable, WaveformTable, SynthTable sub-tables (via data_t offsets)
  4. Build _waveforms vector from WaveformTable
  5. For each cue: resolve waveform via SynthTable ReferenceItems blob
  6. For each track: resolve waveform similarly (1:1 with SynthTable rows)
  7. Parse CueNameTable to associate names with cues
  8. Store _raw_data span for later audio extraction

ACB::internal_awb():
  Lazily parse AFS2 from _awb_file subspan on first call

ACB::extract_waveform(id, out_buffer):
  1. find_waveform(id) → get streaming flag + waveformId
  2. streaming=false: find entry in internal AFS2 by waveformId
  3. Copy bytes from _raw_data[entry.aligned .. entry.aligned+entry.size] into out_buffer

ACB::waveform_span(id):
  Same as extract_waveform but returns a non-owning span (zero-copy)
```

## acbplay CLI

```
acbplay --cpk Unicorn.CPK --acb Sound/bgm.acb --list     # list cues + waveforms
acbplay --cpk Unicorn.CPK --acb Sound/bgm.acb --cue 1001 # play cue 1001
acbplay --cpk Unicorn.CPK --acb Sound/bgm.acb --track 5  # play track 5
```

Links: `criware` + `Boost::program_options` + `SDL3::SDL3`.

Audio is pushed through an `AudioSource` abstraction (span-based for internal AFS2,
file-based for external .awb) to an SDL3 audio stream opened with
`SDL_OpenAudioDeviceStream()`. Currently plays raw HCA/ADX bytes as S16LE PCM —
audio correctness work (HCA decoding) is future work.
