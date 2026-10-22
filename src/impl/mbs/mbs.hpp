#pragma once

// FMBS / V77 animation container.
//
// The file starts with a packed header and a descriptor table for twelve row
// sections. A descriptor supplies the row count, the row width, and the file
// offset of the row data. Section references are indexes into the corresponding
// vector in `mbs::v77`; absent sections have zero rows.
//
// The same container is used for character sprites and background scenes. A
// character track is normally one animation selected by game logic. Background
// tracks are either independent landscape layers or mutually-exclusive states;
// which tracks are live, and how the camera frames them, is external game
// logic rather than a property of this file. Track names are asset-defined.
//
// Cross-section model:
//
//   section_9 (track)
//     -> section_a (sequence)
//          -> section_8 (frame)
//               -> section_6 (keyframe)
//                    -> section_4 (layer) -> section_0/1/2 (colours, UV,
//                                                          vertices)
//                    -> section_5 (hitbox entry) -> section_3 (hitbox geometry)
//               -> section_7 (transform)
//
// `section_b` is shared by character transition tracks and background timing
// tracks. Character transition modes select the next enabled section_9 track;
// background timing rows are a different mode and do not cause track
// transitions. See `sections.hpp` for the field-level contract.
//
// Background notes: section_4.tex_id indexes the FTX texture array, and
// section_2 vertices are the geometry source for content bounds. Background
// layer depth order is not established by section_9 order or by section_7 fog
// alpha; that is OPEN. The caller selects which background tracks are live.
//
// Unknowns are marked `OPEN` on the affected field or enum value. The reader
// should treat those as unresolved rather than as format constants.

#include <cstdint>
#include <span>

namespace mbs {

struct v77;

void parse_v76(std::span<const uint8_t>, v77&);
void parse_v77(std::span<const uint8_t>, v77&);

} // namespace mbs
