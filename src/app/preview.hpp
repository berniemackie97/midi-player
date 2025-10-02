// src/app/preview.hpp
// Pretty, compact console preview of a parsed MIDI song.
// - Prints SMF header summary
// - Prints first 10 NoteOn/NoteOff events with timestamps (s)

#pragma once
#include <iomanip>
#include <iostream>

#include "midi/events.hpp"
#include "midi/tempo.hpp"

namespace app {

inline void print_preview(const midi::Song &song, const midi::TempoMap &tempo) {
  // Header
  std::cout << "SMF header:\n";
  std::cout << "  format  = " << song.header.format << "\n";
  std::cout << "  nTracks = " << song.header.nTracks << "\n";
  if (song.header.isPPQN) {
    std::cout << "  PPQN    = " << song.header.ppqn << " ticks/qn\n";
  } else {
    std::cout << "  SMPTE   = " << song.header.smpte_fps << " fps, "
              << song.header.smpte_sub << " subframes\n";
  }

  // First 10 notes
  std::cout << "\nFirst 10 note events with time:\n";
  const std::size_t limit = std::min<std::size_t>(10, song.notes.size());
  for (std::size_t i = 0; i < limit; ++i) {
    const auto &ev = song.notes[i];
    const double t = midi::ticks_to_seconds(ev.tick, tempo);
    std::cout << "t=" << std::fixed << std::setprecision(3) << t << "s  "
              << (ev.type == midi::EvType::NoteOn ? "On " : "Off")
              << " ch=" << int(ev.ch) << " note=" << int(ev.note)
              << " vel=" << int(ev.vel) << "\n";
  }
}

} // namespace app
