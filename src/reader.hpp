// src/reader.hpp
// A tiny "cursor" that walks a byte buffer safely.
// It gives you: read 1 byte (u8), read big-endian 16-bit (be16), big-endian
// 32-bit (be32), and skip(n).
#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

struct Bytes {
  // We *own* the data here for simplicity. (You could also hold a pointer+size
  // to external data.)
  std::vector<uint8_t> data;
  std::size_t off = 0; // current read position ( 0 = start of file)

  // Construct from an existing buffer by copying it in.
  explicit Bytes(const std::vector<std::uint8_t> &src) : data(src), off(0) {}

  // Read 1 byte, throw if wed run past the end.
  std::uint8_t u8() {
    if (off + 1 > data.size()) {
      throw std::runtime_error("EOF while reading u8");
    }
    return data[off++];
  }

  // Read big-endian 16 bit value (two bytes: hi then low)
  std::uint16_t be16() {
    if (off + 2 > data.size()) {
      throw std::runtime_error("EOF while reading be16");
    }
    std::uint16_t hi = data[off];
    std::uint16_t low = data[off + 1];
    off += 2;
    return static_cast<std::uint16_t>((hi << 8) | low);
  }

  // Read big-endian 32 bit value (4 byte: b0 b1 b2 b3)
  std::uint32_t be32() {
    if (off + 4 > data.size()) {
      throw std::runtime_error("EOF while reading be32");
    }
    std::uint32_t b0 = data[off];
    std::uint32_t b1 = data[off + 1];
    std::uint32_t b2 = data[off + 2];
    std::uint32_t b3 = data[off + 3];
    off += 4;
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  }

  // Advance by n bytes without reading them.
  void skip(std::size_t n) {
    if (off + n > data.size()) {
      throw std::runtime_error("EOF while skipping bytes");
    }
    off += n;
  }
};

// Read a MIDI VLQ (Variable Length Quantity)
// Each byte contributes 7 bits; high bit 1 meand "more byte"
inline std::uint32_t read_vlq(Bytes &r) {
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    std::uint8_t b = r.u8();
    v = (v << 7) | (b & 0x7F);
    if ((b & 0x80) == 0) {
      break;
    }
  }
  return v;
}
