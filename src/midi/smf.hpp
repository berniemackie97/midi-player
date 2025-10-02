// src/midi/smf.hpp
// Public API: parse a Standard MIDI File (SMF) from memory into a Song.
// - No printing here; pure data extraction.
// - Throws std::runtime_error on malformed input.

#pragma once
#include <cstdint>
#include <vector>

#include "midi/events.hpp"

namespace midi {

// Parse an entire Standard MIDI File (SMF) already loaded in memory.
// On success, returns a Song containing:
//   - header: SMFHeader (format, nTracks, timing division info)
//   - notes : flattened NoteOn/NoteOff events across tracks (absolute ticks)
//   - tempi : collected tempo changes (microseconds per quarter note)
// On failure, throws std::runtime_error with a descriptive message.
Song parse_smf(const std::vector<std::uint8_t> &bytes);

} // namespace midi
