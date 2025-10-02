// src/app/cli.hpp
// Minimal, robust CLI parsing for our tiny main.
// Responsibilities:
//  - Extract the positional MIDI path.
//  - Parse an optional --sf <name-or-path> override.
//  - Validate that the MIDI file exists (fail early with a clear error).
//
// Design notes:
//  * This is header-only for now to keep wiring simple (no linking step).
//  * We throw std::runtime_error on problems; main() catches and prints.
//
// Usage from main.cpp:
//   app::Cli cli = app::parse_cli(argc, argv);
//   cli.midiPath     --> std::filesystem::path to the .mid file
//   cli.sfOverride   --> std::optional<std::string> (empty if not provided)

#pragma once
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace app {

struct Cli {
  std::filesystem::path midiPath;
  std::optional<std::string> sfOverride; // from --sf <name-or-path>, if given
};

// Small helper: true if s looks like a flag (starts with '-' and not just "-")
inline bool is_flag_like(const std::string &s) {
  return !s.empty() && s[0] == '-' && s != "-";
}

// Parse argv into our Cli struct.
// Contract:
//  - argv[1] must be the MIDI file path (positional).
//  - Optional: --sf <name-or-path>
//  - Throws std::runtime_error on any invalid input.
inline Cli parse_cli(int argc, char **argv) {
  if (argc < 2) {
    throw std::runtime_error("Usage: " + std::string(argv[0]) +
                             " <file.mid> [--sf <name-or-path>]");
  }

  // 1) Positional MIDI path
  std::filesystem::path midiPath = argv[1];
  if (is_flag_like(midiPath.string())) {
    throw std::runtime_error(
        "First argument must be a MIDI file path, not a flag.");
  }
  if (!std::filesystem::exists(midiPath) ||
      !std::filesystem::is_regular_file(midiPath)) {
    throw std::runtime_error("MIDI file not found: " + midiPath.string());
  }

  // 2) Optional flags
  std::optional<std::string> sfOverride;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      throw std::runtime_error(
          "Usage:\n  " + std::string(argv[0]) +
          " <file.mid> [--sf <name-or-path>]\n"
          "Options:\n"
          "  --sf <name-or-path>  Choose a specific SoundFont by name (in root "
          "soundfonts/) or by path\n");
    } else if (a == "--sf") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--sf requires a value (name or path)");
      }
      sfOverride = std::string(argv[++i]);
    } else {
      // Future flags could go here; for now treat unknowns as errors to avoid
      // surprises.
      throw std::runtime_error("Unknown option: " + a);
    }
  }

  // 3) Return the parsed/validated CLI
  Cli cli;
  cli.midiPath = std::filesystem::canonical(midiPath); // nice absolute path
  cli.sfOverride = sfOverride;
  return cli;
}

} // namespace app
