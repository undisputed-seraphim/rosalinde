# Viewer audio

The viewer plays script-driven audio from the CPK `Sound/` banks:

- `bgm.acb` + `bgm.awb` — background music.
- `se.acb` + `se.awb` — sound effects.
- `voice_jp.acb`, `voice_btl_jp.acb`, `voice_add_jp.acb` — character and battle
  voices with their streaming AWBs.

`viewer/src/audio_banks.cpp` loads each ACB lazily, binds external AWB headers by
`StreamAwbHash` port, and decodes HCA or Nintendo Opus waveforms through
`criware`. Decoded cues are cached as interleaved stereo float at 48 kHz. Cues
that carry several waveforms are mixed as one sound.

## Script events

`asb::RuntimeAudio` is produced by the native shim and consumed by
`DramaSession::advance()`:

| Event | Bus |
|-------|-----|
| `bgm`, `bgm_time`, `bgm_default`, `battle_bgm`, `stage_bgm` | BGM |
| `se`, `action_cue` | SE (falls back to the voice bank for voice cue IDs) |
| `se_chara` | Voice |
| `stop_bgm` | BGM |
| `stop_se` | SE and Voice |
| `bgm_volume`, `se_volume`, `amb_volume`, `amb_fade`, `thunder` | bus volume |

`PlayBGM` fades and `StopBGM`/`StopSE` fades are interpreted in 60 Hz scene
frames. ACB `LoopFlag == 2` marks a cue as looping; loop points come from
`WaveformExtensionDataTable`.

## Dialogue voices

`Msg`/`MsgAsync`/`SelectMsg` receive a message-ID array. `Registry` forwards
those IDs through the host's `set_play_voice` callback. The viewer resolves each
ID through `viewer/data/voice_line_table.tsv`, which maps
`UcScriptMsgConv.fms` text indices to `voice_jp.acb` cue IDs, then plays the cue
on the voice bus. Cues that are already active are not restarted. Narration
created with `CreateSpriteText` is driven by the script's own `PlaySE` calls, so
the viewer does not duplicate it.

The table is stored raw, one row per binary entry. `VoiceLines` sorts it by text
index and uses `std::lower_bound`, so the first entry for a duplicated index
wins just like the game's binary search. The file is extracted from the game
executable:

```bash
python3 tools/extract_voice_lines.py \
  --elf ~/zwei/unicorn_overlord/main2.elf \
  --out viewer/data/voice_line_table.tsv
```

Script `$fact(int)` calls produce a zero-filled array whose **length** is the
message ID; the registry records that length instead of the zero elements.
IDs below `0x10000` are local script message IDs and are shifted by `0x10000`;
larger IDs are already global text indices.

## Message timing

The viewer parses `DbReg/DbRegDb.drg` and `DbReg/DbReg.drg` at runtime with the
`dbreg` parser in `src/impl`. The message auto-advance defaults are:

| Key | Default | Use |
|-----|--------:|-----|
| `VOICE_WAIT_MIN` | 90 | minimum voice wait, frames |
| `PERIOD_WAIT_TIME_VOICE_OFF` | 2 | text wait per rendered character, normal message |
| `PERIOD_WAIT_TIME_VOICE_OFF_MIN` | 30 | minimum text wait, frames |
| `PERIOD_WAIT_TIME_VOICE_OFF` (second entry) | 7 | text wait per character, sprite/demo text |
| `PERIOD_WAIT_TIME_VOICE_ON` | 20 | parsed; not used by the normal `Msg` timer |
| `SELECTMSG_WAIT` | 80 | parsed for selection-message timing |

For a voiced `Msg`, the wait is:

```text
max(voice_length_frames, VOICE_WAIT_MIN)
+ max(PERIOD_WAIT_TIME_VOICE_OFF * rendered_chars,
      PERIOD_WAIT_TIME_VOICE_OFF_MIN)
```

Messages without a voice use only the text term. `CreateSpriteText` uses the
second `PERIOD_WAIT_TIME_VOICE_OFF` entry, or the script's explicit
`_nPeriodWaitTimer` when it is non-negative.

## Flags

| Flag | Effect |
|------|--------|
| `--audio` | Enable audio. Interactive playback is on by default; screenshot/capture runs leave it off. |
| `--no-audio` | Disable audio. |
| `--audio-volume V` | Master volume in `0..1`. |
| `--audio-wav FILE` | Render the scene audio timeline to a 48 kHz stereo PCM16 WAV. |

`--headless` does not open a playback device. It still simulates the audio
timeline when `--audio` or `--audio-wav` is present, so fades, cue selection, and
missing assets can be inspected without hardware.

`GetSoundLength` resolves a cue through the loaded banks and converts its
sample count to 60 Hz frames. There is no synthetic cue-ID offset.

## Diagnostics

`--state`, `--state-json`, `--probe`, and `--record` include an `audio_engine`
block with the active engine mode, live voice count, current BGM cue, event
count, rendered sample frames, cache size, and WAV path. The text state also
contains an `audio:` line.
