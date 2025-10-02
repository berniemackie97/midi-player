// src/midi/tempo.cpp
// Implementation of timing utilities.

#include "midi/tempo.hpp"

#include <algorithm>
#include <vector>

namespace midi {

TempoMap build_tempo_map(const Song &song) {
  // Decide PPQN (ticks per quarter note)
  const unsigned ppqn = song.header.isPPQN
                            ? song.header.ppqn
                            : 480; // SMPTE: simple fallback for now

  // Work on a copy so we can sort safely
  std::vector<TempoEv> tempi = song.tempi;
  std::sort(tempi.begin(), tempi.end(),
            [](const TempoEv &a, const TempoEv &b) { return a.tick < b.tick; });

  // Default tempo is 120 BPM => 500,000 microseconds per quarter note
  double current_usPerQN = 500000.0;
  double accSec = 0.0;
  std::uint32_t lastTick = 0;

  TempoMap map;
  map.ppqn = ppqn;
  map.segments.clear();
  map.segments.push_back(TempoSeg{0u, 0.0, current_usPerQN});

  for (const auto &t : tempi) {
    if (t.tick < lastTick) {
      // Guard against malformed or out-of-order events
      continue;
    }

    // Advance accumulated seconds from lastTick to this tempo-change tick
    const double deltaQN = (t.tick - lastTick) / static_cast<double>(ppqn);
    accSec += deltaQN * (current_usPerQN * 1e-6);

    // Start a new segment at this tick with the new tempo
    current_usPerQN = static_cast<double>(t.usPerQN);
    lastTick = t.tick;
    map.segments.push_back(TempoSeg{t.tick, accSec, current_usPerQN});
  }

  return map;
}

double ticks_to_seconds(std::uint32_t tick, const TempoMap &tempo) {
  // Find the last segment whose startTick <= tick (linear scan is fine; lists
  // are tiny)
  const TempoSeg *seg = &tempo.segments.front();
  for (const auto &s : tempo.segments) {
    if (s.startTick <= tick)
      seg = &s;
    else
      break;
  }

  const double deltaQN =
      (tick - seg->startTick) / static_cast<double>(tempo.ppqn);
  return seg->startSec + deltaQN * (seg->usPerQN * 1e-6);
}

} // namespace midi
