// src/reader.hpp
// Tiny safe cursor for big-endian reads + MIDI VLQ.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

struct Bytes {
  std::vector<std::uint8_t> data;
  std::size_t off = 0; // current read position

  explicit Bytes(const std::vector<std::uint8_t> &src) : data(src), off(0) {}

  [[nodiscard]] std::uint8_t u8() {
    if (off + 1 > data.size())
      throw std::runtime_error("EOF while reading u8");
    return data[off++];
  }

  [[nodiscard]] std::uint16_t be16() {
    if (off + 2 > data.size())
      throw std::runtime_error("EOF while reading be16");
    std::uint16_t hi = data[off], lo = data[off + 1];
    off += 2;
    return static_cast<std::uint16_t>((hi << 8) | lo);
  }

  [[nodiscard]] std::uint32_t be32() {
    if (off + 4 > data.size())
      throw std::runtime_error("EOF while reading be32");
    std::uint32_t b0 = data[off], b1 = data[off + 1], b2 = data[off + 2],
                  b3 = data[off + 3];
    off += 4;
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  }

  void skip(std::size_t n) {
    if (off + n > data.size())
      throw std::runtime_error("EOF while skipping bytes");
    off += n;
  }
};

// Read a MIDI VLQ (Variable Length Quantity).
inline std::uint32_t read_vlq(Bytes &r) {
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    std::uint8_t b = r.u8();
    v = (v << 7) | (b & 0x7F);
    if ((b & 0x80) == 0)
      break; // high bit 0 => last byte
  }
  return v;
}
