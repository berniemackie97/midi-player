// src/main.cpp
// Tiny orchestration-only entry point.
// - Parse CLI (MIDI path + optional --sf override)
// - Read MIDI bytes
// - Parse to Song (header + events)
// - Build a tempo map
// - Pick a SoundFont (root soundfonts/ first; --sf wins)
// - Print a small preview
//
// Implementation details live in modules:
//   app/cli.hpp              -> app::parse_cli
//   io/io.hpp                -> io::read_all
//   midi/smf.hpp             -> midi::parse_smf
//   midi/tempo.hpp           -> midi::TempoMap, midi::build_tempo_map
//   assets/sf_resolver.hpp   -> assets::select_soundfont
//   app/preview.hpp          -> app::print_preview

#include "app/cli.hpp"
#include "app/preview.hpp"
#include "assets/sf_resolver.hpp"
#include "io/io.hpp"
#include "midi/smf.hpp"
#include "midi/tempo.hpp"

#include <iostream>

int main(int argc, char **argv) {
  try {
    app::Cli cli = app::parse_cli(argc, argv);

    auto midiBytes = io::read_all(cli.midiPath);
    midi::Song song = midi::parse_smf(midiBytes);       // header + events
    midi::TempoMap tempo = midi::build_tempo_map(song); // normalized timing

    auto sf = assets::select_soundfont(cli.sfOverride, argv[0]);
    std::cout << "SoundFont: " << sf.string() << "\n";

    app::print_preview(song, tempo); // small header + first 10 notes
    // later: player::play(song, tempo, sf);

    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
