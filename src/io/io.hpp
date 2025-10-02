// src/io/io.hpp
// Thin I/O façade for reading whole files.
// We reuse your existing util.hpp::read_all and expose it as io::read_all.
//
// Why this exists:
//  - Keeps main and higher layers depending on a stable interface
//  (io::read_all)
//  - Lets us swap the implementation later (mmap, memory-mapped I/O, etc.)
//  - Avoids duplicating logic you already wrote.
//
// Usage:
//   auto bytes = io::read_all(path);          // path: std::filesystem::path or
//   std::string
//
// Throws std::runtime_error on errors (propagated from util.hpp).

#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "common/util.hpp"

namespace io {

// Overload: std::string
inline std::vector<std::uint8_t> read_all(const std::string &path) {
  return ::read_all(path);
}

// Overload: std::filesystem::path
inline std::vector<std::uint8_t> read_all(const std::filesystem::path &p) {
  return ::read_all(p.string());
}

} // namespace io
