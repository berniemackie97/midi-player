// src/midi/tempo.hpp
// Timing utilities: build a tempo map and convert ticks -> seconds.
//
// Contract:
//  - build_tempo_map(const Song&): consumes Song.header + Song.tempi
//      * Assumes PPQN timing. If the file uses SMPTE timing, we currently
//        fall back to a default PPQN (480) inside the implementation.
//  - ticks_to_seconds(tick, TempoMap): converts absolute tick to seconds.
//
// Notes:
//  - TempoMap carries PPQN and a list of segments (startTick/startSec/usPerQN).
//  - We'll keep the API minimal and stable; playback and rendering can use it
//    without caring about how segments are stored.

#pragma once
#include "midi/events.hpp"

#include <cstdint>

namespace midi {

// Build a tempo map from a parsed Song.
// - Uses Song.header.ppqn when header.isPPQN == true.
// - If the file uses SMPTE timing (header.isPPQN == false), we currently
//   approximate with ppqn = 480 (common default). Proper SMPTE will be added
//   later.
TempoMap build_tempo_map(const Song &song);

// Convert an absolute tick to seconds using the TempoMap.
// - Works for any tick within or after the last segment: beyond the last
//   tempo change we continue with the last tempo.
double ticks_to_seconds(std::uint32_t tick, const TempoMap &tempo);

} // namespace midi
