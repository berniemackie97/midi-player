// src/util.hpp
// Small helper to read an entire file into a byte vector (binary mode).
#pragma once
#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

inline std::vector<std::uint8_t> read_all(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("Could not open file: " + path);
  }
  f.seekg(0, std::ios::end);
  std::streamsize sz = f.tellg();
  if (sz < 0) {
    throw std::runtime_error("Could not get size of file: " + path);
  }
  std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
  f.seekg(0, std::ios::beg);
  if (sz && !f.read(reinterpret_cast<char *>(buf.data()), sz)) {
    throw std::runtime_error("Could not read file: " + path);
  }
  return buf;
}
