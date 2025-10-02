// src/midi/smf.cpp
// Parse a Standard MIDI File (SMF) from memory into midi::Song.
// Pure parsing: no printing, no I/O.

#include "midi/smf.hpp"
#include "common/reader.hpp" // Bytes cursor + read_vlq()
#include "midi/events.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// Parse SMF header (MThd chunk) and fill midi::SMFHeader.
midi::SMFHeader parse_header(Bytes &r) {
  const std::uint32_t id = r.be32();
  if (id != 0x4d546864) {
    throw std::runtime_error("Not a MIDI file (missing 'MThd')");
  }

  const std::uint32_t length = r.be32();
  if (length != 6) {
    throw std::runtime_error("Header chunk length must be 6");
  }

  midi::SMFHeader h{};
  h.format = r.be16();
  h.nTracks = r.be16();
  h.division = r.be16();

  if ((h.division & 0x8000) == 0) {
    // PPQN timing (ticks per quarter note)
    h.isPPQN = true;
    h.ppqn = static_cast<unsigned>(h.division & 0x7FFF);
  } else {
    // SMPTE timing (two's-complement FPS in high byte, subframes in low byte)
    h.isPPQN = false;
    h.smpte_fps = 256 - ((h.division >> 8) & 0xFF); // e.g., 24, 25, 29, 30
    h.smpte_sub = static_cast<int>(h.division & 0xFF);
  }

  return h; // r.off now points to first track chunk (MTrk)
}

// Copy a slice of the file into a new Bytes cursor for a track.
Bytes make_slice(const std::vector<std::uint8_t> &file, std::size_t start,
                 std::size_t len) {
  if (start + len > file.size()) {
    throw std::runtime_error("Track slice out of range");
  }
  std::vector<std::uint8_t> tmp(file.begin() + start,
                                file.begin() + start + len);
  return Bytes(tmp);
}

// Walk a single MTrk chunk and append events to out vectors.
// - Produces absolute tick times (track-local absolute; OK for format 1).
void walk_one_track(const std::vector<std::uint8_t> &file, Bytes &r,
                    int /*trackIndex*/, std::vector<midi::NoteEv> &outNotes,
                    std::vector<midi::TempoEv> &outTempi) {
  const std::uint32_t id = r.be32();
  if (id != 0x4D54726B) { // "MTrk"
    throw std::runtime_error("Missing 'MTrk' chunk");
  }
  const std::uint32_t len = r.be32();

  // Make a sub-cursor for just this track's bytes, then skip over them in the
  // main reader.
  const std::size_t trackStart = r.off;
  Bytes tr = make_slice(file, trackStart, len);
  r.skip(len);

  std::uint32_t tick = 0;
  std::uint8_t running = 0; // last seen channel status for running status

  while (tr.off < tr.data.size()) {
    // 1) Delta-time (Variable-Length Quantity)
    std::uint32_t delta = read_vlq(tr);
    tick += delta;

    // 2) Status or running status?
    std::uint8_t first = tr.u8();
    std::uint8_t status = 0;
    bool haveData1 = false;
    std::uint8_t data1 = 0;

    if (first & 0x80) {
      // New status byte
      status = first;
      if ((status & 0xF0) < 0xF0) {
        running = status; // only channel messages set running status
      }
    } else {
      // Running status: 'first' is actually data1 for the previous channel
      // status
      if (running == 0) {
        throw std::runtime_error("Running status used before any status");
      }
      status = running;
      haveData1 = true;
      data1 = first;
    }

    const std::uint8_t type = status & 0xF0;
    const std::uint8_t ch = status & 0x0F;

    // Channel messages with two data bytes
    if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 ||
        type == 0xE0) {
      std::uint8_t d1 = haveData1 ? data1 : tr.u8();
      std::uint8_t d2 = tr.u8();

      if (type == 0x90 && d2 != 0) {
        // Note On
        outNotes.push_back(
            midi::NoteEv{tick, ch, d1, d2, midi::EvType::NoteOn});
      } else if (type == 0x80 || (type == 0x90 && d2 == 0)) {
        // Note Off (either true 0x80 or "Note On with velocity 0")
        outNotes.push_back(
            midi::NoteEv{tick, ch, d1, d2, midi::EvType::NoteOff});
      } else {
        // Other two-byte channel messages (Poly AT, CC, Pitch Bend) – ignore
        // for now
      }
      continue;
    }

    // Channel messages with one data byte
    if (type == 0xC0 || type == 0xD0) {
      std::uint8_t d1 = haveData1 ? data1 : tr.u8();
      (void)d1; // Program Change / Channel Pressure – ignore for now
      continue;
    }

    // Meta events
    if (status == 0xFF) {
      std::uint8_t metaType = tr.u8();
      std::uint32_t mlen = read_vlq(tr);

      if (metaType == 0x2F) { // End of Track
        if (mlen != 0)
          tr.skip(mlen);
        break;
      } else if (metaType == 0x51 && mlen == 3) {
        // Tempo: 3 bytes big-endian microseconds per quarter note
        std::uint32_t t0 = tr.u8(), t1 = tr.u8(), t2 = tr.u8();
        std::uint32_t usPerQN = (t0 << 16) | (t1 << 8) | t2;
        outTempi.push_back(midi::TempoEv{tick, usPerQN});
      } else {
        // Skip other meta payloads we don't consume yet
        tr.skip(mlen);
      }
      continue;
    }

    // SysEx events
    if (status == 0xF0 || status == 0xF7) {
      std::uint32_t slen = read_vlq(tr);
      tr.skip(slen);
      continue;
    }

    // Anything else is unsupported/malformed at this stage
    std::ostringstream oss;
    oss << "Unsupported or malformed status byte: 0x" << std::hex
        << int(status);
    throw std::runtime_error(oss.str());
  }
}

} // namespace

namespace midi {

Song parse_smf(const std::vector<std::uint8_t> &bytes) {
  Bytes r(bytes);

  // Header
  SMFHeader header = parse_header(r);

  // Accumulate events from all tracks
  std::vector<NoteEv> notes;
  std::vector<TempoEv> tempi;
  notes.reserve(4096);
  tempi.reserve(64);

  for (std::uint16_t i = 0; i < header.nTracks; ++i) {
    walk_one_track(bytes, r, static_cast<int>(i), notes, tempi);
  }

  Song song;
  song.header = header;
  song.notes = std::move(notes);
  song.tempi = std::move(tempi);
  return song;
}

} // namespace midi
