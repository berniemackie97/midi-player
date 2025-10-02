// src/midi/events.hpp
// Core MIDI domain types shared across the app.
// Keep this header light: plain structs, no implementation details.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace midi {

// --- Basic event kinds we care about for now ---
enum class EvType { NoteOn, NoteOff };

// A channel note event (Note On/Off)
struct NoteEv {
  std::uint32_t tick; // absolute tick in its track timeline
  std::uint8_t ch;    // MIDI channel 0..15
  std::uint8_t note;  // MIDI note number 0..127
  std::uint8_t vel;   // velocity 0..127 (0 + NoteOn == NoteOff)
  EvType type;
};

// A tempo meta event: microseconds per quarter note at a given tick
struct TempoEv {
  std::uint32_t tick;    // absolute tick where tempo takes effect
  std::uint32_t usPerQN; // microseconds per quarter note
};

// Parsed SMF header (subset we need)
struct SMFHeader {
  std::uint16_t format = 0;   // 0, 1, or 2
  std::uint16_t nTracks = 0;  // number of track chunks
  std::uint16_t division = 0; // raw division field

  bool isPPQN = true;  // true if PPQN timing, false if SMPTE
  unsigned ppqn = 480; // valid when isPPQN == true
  int smpte_fps = 0;   // valid when isPPQN == false
  int smpte_sub = 0;   // valid when isPPQN == false
};

// A lightweight container for the parsed song: header + extracted events.
struct Song {
  SMFHeader header;
  std::vector<NoteEv> notes;  // flattened across tracks (absolute ticks)
  std::vector<TempoEv> tempi; // collected from all tracks (sorted later)
  // (If you later want per-track separation, we can add tracks[] of events.)
};

// A precomputed timing map to convert ticks -> seconds under tempo changes.
struct TempoSeg {
  std::uint32_t startTick = 0; // segment begins at this absolute tick
  double startSec = 0;         // time in seconds at startTick
  double usPerQN = 500000.0;   // tempo in this segment
};

// A thin wrapper for tempo info; keeps room for future metadata.
struct TempoMap {
  unsigned ppqn = 480;            // ticks per quarter note
  std::vector<TempoSeg> segments; // ascending by startTick
};

} // namespace midi
