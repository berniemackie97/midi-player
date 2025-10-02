// src/main.cpp
// Milestone 1: parse the SMF (Standard MIDI File) header and print key fields.
// Teaches: chunk structure, big-endian integers, PPQN vs SMPTE division.

#include "reader.hpp"
#include "util.hpp"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

enum class EvType { NoteOn, NoteOff };

struct NoteEv {
  std::uint32_t tick;
  std::uint8_t ch;
  std::uint8_t note;
  std::uint8_t vel;
  EvType type;
};

struct TempoEv {
  std::uint32_t tick;
  std::uint32_t usPerQN;
};

struct SMFHeader {
  std::uint16_t format;
  std::uint16_t nTracks;
  std::uint16_t division; // raw division field
  bool isPPQN;            // true if PPQN, false if SMPTE
  unsigned ppqn;          // valid if isPPQN
  int smpte_fps;          // valid if !isPPQN
  int smpte_sub;          // valid if !isPPQN
};

static SMFHeader parse_header(Bytes &r) {
  // Every SMF starts with a "header chunk":
  // 4 bytes: "MThd" (0x4d 0x54 0x68 0x64)
  // 4 bytes: length (big-endian) - must be 6
  // 6 bytes: payload: format (2), nTracks (2), division (2)
  const std::uint32_t id = r.be32();
  if (id != 0x4d546864) {
    throw std::runtime_error("Not a MIDI file");
  }

  const std::uint32_t length = r.be32();
  if (length != 6) {
    throw std::runtime_error("Header chunk must be 6 bytes long");
  }

  SMFHeader h{};
  h.format = r.be16();
  h.nTracks = r.be16();
  h.division = r.be16();

  std::cout << "SMF header:\n";
  std::cout << "  format  = " << h.format << "\n";
  std::cout << "  nTracks  = " << h.nTracks << "\n";

  // division: if high bit is 0 → PPQN (ticks per quarter note)
  // if high bit is 1 → SMPTE timing (frames/subframes)
  if ((h.division & 0x8000) == 0) {
    h.isPPQN = true;
    h.ppqn = h.division & 0x7FFF;
    std::cout << "  PPQN  = " << h.ppqn << " ticks/qn\n";
  } else {
    // SMPTE: top byte is -frames-per-second (two's complement), low byte is
    // subframes per frame.
    h.isPPQN = false;
    h.smpte_fps = 256 - ((h.division >> 8) & 0xFF); // e.g, 24, 25,, 29, 30
    h.smpte_sub = h.division & 0xFF;
    ;
    std::cout << "  SMPTE  = " << h.smpte_fps << " fps, " << h.smpte_sub
              << " subframes\n";
  }
  return h;
  // At this point r.off points to the *start of the first track chunk* (MTrk)
}

// Return a new Bytes cursor that contains exactly [start, start+len) of the
// original file. This copies the slice (simple and fine for small MIDI files).
static Bytes make_slice(const std::vector<std::uint8_t> &file,
                        std::size_t start, std::size_t len) {
  if (start + len > file.size())
    throw std::runtime_error("track slice out of range");
  std::vector<std::uint8_t> tmp(file.begin() + start,
                                file.begin() + start + len);
  return Bytes(tmp);
}

static void walk_one_track(const std::vector<std::uint8_t> &file, Bytes &r,
                           int trackIndex, std::vector<NoteEv> &outNotes,
                           std::vector<TempoEv> &outTempi) {
  // Track chunk header: "MTrk" + 32-bit big-endian length
  const std::uint32_t id = r.be32();
  if (id != 0x4D54726B)
    throw std::runtime_error("Missing 'MTrk' for track " +
                             std::to_string(trackIndex));
  const std::uint32_t len = r.be32();

  // Create a sub-cursor for the track data.
  const std::size_t trackStart = r.off; // current position in the file
  Bytes tr = make_slice(file, trackStart, len);

  // Advance the main reader past this track so next call sees the next chunk.
  r.skip(len);

  std::cout << "\nTrack " << trackIndex << " (length " << len << " bytes)\n";

  std::uint32_t tick = 0;   // absolute tick time within this track
  std::uint8_t running = 0; // last seen channel status (for running status)

  // Safety: avoid infinite loops on malformed files
  while (tr.off < tr.data.size()) {
    // 1) Delta-time (VLQ)
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
      if ((status & 0xF0) < 0xF0)
        running = status; // only channel messages update running status
    } else {
      // Running status: 'first' is actually data1 for the previous channel
      // status.
      if (running == 0)
        throw std::runtime_error("Running status used before any status");
      status = running;
      haveData1 = true;
      data1 = first;
    }

    // 3) Dispatch
    const std::uint8_t type = status & 0xF0;
    const std::uint8_t ch = status & 0x0F;

    if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 ||
        type == 0xE0) {
      // Two data bytes
      std::uint8_t d1 = haveData1 ? data1 : tr.u8();
      std::uint8_t d2 = tr.u8();

      if (type == 0x90 && d2 != 0) {
        // NoteOn
        outNotes.push_back(NoteEv{tick, ch, d1, d2, EvType::NoteOn});
        std::cout << "tick " << tick << "  NoteOn  ch=" << int(ch)
                  << " note=" << int(d1) << " vel=" << int(d2) << "\n";
      } else if (type == 0x80 || (type == 0x90 && d2 == 0)) {
        // NoteOff (either true 0x80 or 0x90 with velocity 0)
        outNotes.push_back(NoteEv{tick, ch, d1, d2, EvType::NoteOff});
        std::cout << "tick " << tick << "  NoteOff ch=" << int(ch)
                  << " note=" << int(d1) << " vel=" << int(d2) << "\n";
      } else if (type == 0xB0) {
        // Control Change
        // std::cout << "tick " << tick << "  CC     ch="<<int(ch)<<"
        // num="<<int(d1)<<" val="<<int(d2)<<"\n";
      }
      continue;
    }

    if (type == 0xC0 || type == 0xD0) {
      // One data byte (Program Change / Channel Pressure)
      std::uint8_t d1 = haveData1 ? data1 : tr.u8();
      if (type == 0xC0) {
        // Program Change
        // std::cout << "tick " << tick << "  Prog   ch="<<int(ch)<<"
        // prog="<<int(d1)<<"\n";
      }
      continue;
    }

    if (status == 0xFF) {
      // Meta event: 0xFF type, length (VLQ), then 'length' data bytes
      std::uint8_t metaType = tr.u8();
      std::uint32_t mlen = read_vlq(tr);

      if (metaType == 0x2F) { // End of Track
        if (mlen != 0)
          tr.skip(mlen);
        std::cout << "tick " << tick << "  <End of Track>\n";
        break;
      } else if (metaType == 0x51 && mlen == 3) {
        // Tempo: 3 bytes big-endian microseconds per quarter note
        std::uint32_t t0 = tr.u8(), t1 = tr.u8(), t2 = tr.u8();
        std::uint32_t usPerQN = (t0 << 16) | (t1 << 8) | t2;
        outTempi.push_back(TempoEv{tick, usPerQN});
        std::cout << "tick " << tick << "  Tempo  " << usPerQN << " us/qn\n";
      } else {
        // Skip other meta types for now
        tr.skip(mlen);
      }
      continue;
    }

    if (status == 0xF0 || status == 0xF7) {
      // SysEx: status, length (VLQ), then data
      std::uint32_t slen = read_vlq(tr);
      tr.skip(slen);
      continue;
    }

    // Any other system message we don't handle here → treat as
    // malformed/unsupported
    std::ostringstream oss;
    oss << "Unsupported or malformed status byte: 0x" << std::hex
        << int(status);
    throw std::runtime_error(oss.str());
  }
}

struct TempoSeg {
  std::uint32_t startTick;
  double startSec;
  double usPerQN;
};

// Create a timeline of tempo segments. Format 1 MIDI usually puts tempo in
// track 0, but we collect from all tracks just in case.
static std::vector<TempoSeg> build_tempo_map(std::vector<TempoEv> tempi,
                                             unsigned ppqn) {
  // Ensure ascending by tick
  std::sort(tempi.begin(), tempi.end(),
            [](const TempoEv &a, const TempoEv &b) { return a.tick < b.tick; });

  // Default tempo is 120 BPM = 500,000 µs/qn until the first FF 51.
  double current_usPerQN = 500000.0;
  double accSec = 0.0;
  std::uint32_t lastTick = 0;

  std::vector<TempoSeg> segs;
  segs.push_back(TempoSeg{0, 0.0, current_usPerQN});

  for (const auto &t : tempi) {
    if (t.tick < lastTick)
      continue; // guard against weird out-of-order data
    // accumulate seconds from lastTick up to t.tick with the previous tempo
    double deltaQN = (t.tick - lastTick) / static_cast<double>(ppqn);
    accSec += deltaQN * (current_usPerQN * 1e-6);

    // Start a new segment at this tempo change
    current_usPerQN = static_cast<double>(t.usPerQN);
    lastTick = t.tick;
    segs.push_back(TempoSeg{t.tick, accSec, current_usPerQN});
  }
  return segs;
}

// Convert an absolute tick into seconds using the tempo map.
static double ticks_to_seconds(std::uint32_t tick,
                               const std::vector<TempoSeg> &segs,
                               unsigned ppqn) {
  // Find the last segment whose startTick <= tick.
  // (Linear scan is fine for now; these arrays are tiny.)
  const TempoSeg *seg = &segs.front();
  for (const auto &s : segs) {
    if (s.startTick <= tick)
      seg = &s;
    else
      break;
  }
  double deltaQN = (tick - seg->startTick) / static_cast<double>(ppqn);
  return seg->startSec + deltaQN * (seg->usPerQN * 1e-6);
}

int main(int argc, char **argv) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: " << argv[0] << "<file.mid>\n";
      return 2;
    }

    const auto buf = read_all(argv[1]); // read file into memory
    Bytes r(buf);
    SMFHeader h = parse_header(r);
    // Collect from all tracks
    std::vector<NoteEv> notes;
    std::vector<TempoEv> tempi;

    for (std::uint16_t i = 0; i < h.nTracks; ++i) {
      walk_one_track(buf, r, static_cast<int>(i), notes, tempi);
    }

    // If no explicit tempo events, we still have an implicit 120 BPM from tick
    // 0.
    auto segs = build_tempo_map(tempi, h.isPPQN ? h.ppqn : 480 /*fallback*/);

    // Demo: print the first 10 note events with seconds
    std::cout << "\nFirst 10 note events with time:\n";
    for (std::size_t i = 0; i < notes.size() && i < 10; ++i) {
      double t = ticks_to_seconds(notes[i].tick, segs, h.ppqn);
      std::cout << "t=" << std::fixed << std::setprecision(3) << t << "s  "
                << (notes[i].type == EvType::NoteOn ? "On " : "Off")
                << " ch=" << int(notes[i].ch) << " note=" << int(notes[i].note)
                << " vel=" << int(notes[i].vel) << "\n";
    }

    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
