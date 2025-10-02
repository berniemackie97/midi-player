// src/main.cpp
// Tiny orchestration: CLI → load bytes → parse → tempo map → choose SF2 →
// preview → play.

#include <filesystem>
#include <iostream>

#include "app/cli.hpp"
#include "app/preview.hpp"
#include "assets/sf_resolver.hpp"
#include "audio/player.hpp"
#include "io/io.hpp"
#include "midi/smf.hpp"
#include "midi/tempo.hpp"

int main(int argc, char **argv) {
  try {
    // 1) Parse CLI (MIDI path + optional --sf <name>)
    app::Cli cli = app::parse_cli(argc, argv);

    // 2) Load file
    const auto bytes = io::read_all(cli.midiPath.string());

    // 3) Parse MIDI and build tempo map
    midi::Song song = midi::parse_smf(bytes);
    midi::TempoMap tempo = midi::build_tempo_map(song);

    // 4) Resolve SoundFont from ./soundfonts/ (default =
    // Sonatina_Symphonic_Orchestra.sf2) NOTE: Pass argv[0] so the resolver can
    // compute the executable directory if needed.
    std::filesystem::path sf =
        assets::select_soundfont(cli.sfOverride, argv[0]);
    std::cout << "SoundFont: " << sf.string() << "\n\n";

    // 5) Quick text preview (header + first 10 note events)
    app::print_preview(song, tempo);

    // 6) Make it sing (blocking until the song finishes)
    audio::play(song, tempo, sf);

    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
