// src/audio/player.hpp
// High-level, blocking MIDI playback using TinySoundFont (tsf) + miniaudio.
// You hand us a parsed Song, a TempoMap, and a SoundFont path.
// We start a playback device, stream audio, and return when the song finishes.
//
// Public API (one function):
//   audio::play(song, tempo, sf2Path);
//
// Design notes:
// - We keep the implementation behind this interface so main.cpp stays tiny.
// - The .cpp contains the single-header library implementations and the device
//   callback, so headers elsewhere stay clean.
// - First cut: GM "piano on every channel". Program Changes are ignored for
// now;
//   we can parse/program-map later without touching this header.

#pragma once
#include <filesystem>

#include "midi/events.hpp"
#include "midi/tempo.hpp"

namespace audio {

// Blocking playback. Throws std::runtime_error on device or SF2 errors.
// This function only returns after playback completes (or on error).
void play(const midi::Song &song, const midi::TempoMap &tempo,
          const std::filesystem::path &sf2Path);

} // namespace audio
