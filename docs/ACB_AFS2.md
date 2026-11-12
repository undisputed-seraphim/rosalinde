# ACB + AFS2 File Format

## Overview

An ACB (CRI ADX2 Cue Binary) is a top-level `@UTF` table containing embedded
sub-tables and an embedded AFS2 archive. Cues reference waveforms directly, or
through synth, sequence, track-event, and block tables.

The Sound banks in Unicorn Overlord use two compressed codecs:

| EncodeType | Format | Container |
|---|---|---|
| 2 | HCA | `HCA\0` stream |
| 24 | Nintendo Opus | `0x80000001` Nintendo Opus header |

## Byte order

- ACB root table and sub-tables: `@UTF`, big-endian.
- AFS2 headers and data: little-endian.

## File structure

```text
ACB
├── @UTF root row
│   ├── Version
│   ├── CueTable / CueNameTable / WaveformTable
│   ├── SynthTable / SequenceTable / TrackTable
│   ├── TrackEventTable / BlockSequenceTable / BlockTable
│   ├── WaveformExtensionDataTable
│   ├── StreamAwbHash
│   └── AwbFile (internal AFS2 blob)
├── embedded @UTF sub-tables
└── internal AFS2 data
```

The companion streaming archive is named by `StreamAwbHash.Name`
(`bgm` → `Sound/bgm.awb`, `voice_jp` → `Sound/voice_jp.awb`, …). The row index
is the AWB port used by `WaveformTable.StreamAwbPortNo`.

## Cue → waveform resolution

Reference types:

- `1`: cue → waveform index directly.
- `2`: cue → `SynthTable` row.
- `3`: cue → `SequenceTable` row.
- `8`: cue → `BlockSequenceTable` row.

The common type-3 chain is:

```text
CueTable.ReferenceIndex
  → SequenceTable[ReferenceIndex].NumTracks/TrackIndex
  → TrackTable[track].EventIndex
  → TrackEventTable[EventIndex].Command
  → TLV: 2000/2003/2004 noteOn (type=2, index=SynthTable row)
  → SynthTable[index].ReferenceItems
  → entries {u16 BE type, u16 BE index}
      type 1 → WaveformTable index
      type 2 → SynthTable index (recursive)
      type 3 → SequenceTable index (recursive)
```

`ReferenceItems` entries are 4 bytes each. The old offset-correction heuristic
was only an approximation; use the table chain above.

A cue may resolve to multiple waveforms (layered tracks). Mix all of them.

## Sub-table fields

### CueTable

```text
CueId            u32
ReferenceType    u8
ReferenceIndex   u16
Length           u32   duration in samples (optional)
```

### WaveformTable

```text
EncodeType       u8
Streaming        u8    0 = internal AWB, 1 = streaming, 2 = prefetch + stream
LoopFlag         u8    2 = loop
NumChannels      u8
SamplingRate     u32
NumSamples       u32
MemoryAwbId      u16   internal AFS2 ID
StreamAwbId      u16   external AFS2 ID
StreamAwbPortNo  u16   AWB port
ExtensionData    u16   WaveformExtensionDataTable row, 0xFFFF = none
Id               u16   older files; replaces Memory/Stream ID when present
```

For streaming 1 and 2 use `StreamAwbId` and the external AWB. For streaming 0
use `MemoryAwbId` and the internal AWB.

### WaveformExtensionDataTable

```text
LoopStart   u32   sample offset
LoopEnd     u32   sample offset
```

### TrackEventTable

`Command` is a TLV stream: `u16 code, u8 length, payload`. Note-on codes
`2000`, `2003`, and `2004` carry a `u16 type, u16 index` reference item.

## AFS2 binary format

### Header (16 bytes, little-endian)

```text
0x00  4  "AFS2"
0x04  u32 version
          offset_field_size = (version >> 8) & 0xFF
          id_field_size     = version & 0xFF (usually 2)
0x08  i32 file_count
0x0C  u32 byte_alignment
          lower 16 bits = alignment
          upper 16 bits = hca_key_modifier
```

The entry stride is `offset_field_size` for both the ID array and the offset
array. libcgss hard-codes a 2-byte ID stride; the Unicorn Overlord files use the
offset field size (4).

```text
ID array:    0x10 + i * stride          u16 ID in the low half
Offset array: 0x10 + count * stride + i * stride
```

### Alignment and size

```text
raw_offset     = offset array entry + archive base
aligned_offset = round_up(raw_offset, alignment)
file_size      = next.raw_offset - this.aligned_offset
last file_size = archive_end - last.aligned_offset
```

Entry data starts at `aligned_offset`, not at `raw_offset`. The bytes between
`raw_offset` and `aligned_offset` are padding.

For a header-only AFS2 read from a large AWB, pass the real file size as the
archive end so the last entry size is still correct.

## Internal and external AWB

- `AwbFile` is an AFS2 archive embedded in the ACB. Its offsets are absolute
  within the ACB buffer.
- Streaming waveforms live in `Sound/<StreamAwbHash.Name>.awb`. Read the AFS2
  header from the AWB, then read only the requested waveform range from the CPK.

`CPK` stores the Sound `.awb` entries uncompressed (`file_size == extract_size`),
so range reads are direct file reads. Compressed entries are extracted once and
sliced.

## HCA

- Header: `HCA\0`, version, header size.
- Chunks: `fmt`, `comp`/`dec`, `vbr`, `ath`, `loop`, `ciph`, `rva`, `comm`.
- Decoder: `libcriware/src/hca/clhca.c` (vendored from vgmstream).
- HCA header and block checksums are validated by the decoder.
- Cipher types 0, 1, and 56 are supported; Unicorn Overlord uses type 0.
- `HCA_INFO` reports encoder delay/padding and loop sample positions.

## Nintendo Opus

Header chunks:

```text
0x00  0x80000001 basic info
0x04  chunk size
0x08  version u8, channels u8 at +0x09
0x0C  sample rate u32le (48000)
0x10  data chunk offset
0x18  optional context chunk offset
0x1C  pre-skip samples u16le
```

Data chunk at `data_offset`:

```text
0x00  0x80000004
0x04  data size u32le
0x08  packet stream
```

Each packet is `u32be packet_size + 4-byte state + Opus packet`. Decode with
libopus at 48 kHz, discard the pre-skip samples, and trim to the ACB waveform
sample count. Optional `0x80000003` context chunks provide loop/sample
information.

## acbplay

```bash
acbplay --cpk Unicorn.CPK --acb Sound/bgm.acb --list
acbplay --cpk Unicorn.CPK --acb Sound/bgm.acb --cue 1 --wav /tmp/bgm1.wav
acbplay --cpk Unicorn.CPK --acb Sound/se.acb --cue 1010
```

`--list` prints cue and waveform metadata. `--cue`/`--track` decode HCA or Opus
to 48 kHz stereo; `--wav` writes a 16-bit WAV instead of opening an audio
device.
