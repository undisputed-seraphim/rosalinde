# FMBS / V77

The FMBS/V77 format is documented with the code that owns it:

- `src/impl/mbs/mbs.hpp` — container layout, section model, cross-section
  references, and parser API.
- `src/impl/mbs/sections.hpp` — packed row definitions, field semantics, flag
  enums, and explicit `OPEN` annotations for unresolved behavior.
- `viewer/src/background_tracks.hpp` — background track selection policy.

Field-level documentation lives next to the fields. This file intentionally does
not keep historical notes, sample asset lists, or corpus statistics.
