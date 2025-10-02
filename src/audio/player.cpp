// src/audio/player.cpp
// Turn parsed MIDI into sound with TinySoundFont + miniaudio.
// Blocking call: returns when the song (plus tail) has finished rendering.

#define TSF_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "tsf.h"

#include "audio/player.hpp"
#include "midi/tempo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

// A scheduled NoteOn/Off in seconds.
struct ScheduledEvent {
  double tSec;       // when to apply, in seconds
  std::uint8_t ch;   // 0..15
  std::uint8_t note; // 0..127
  std::uint8_t vel;  // 0..127
  bool on;           // true=NoteOn, false=NoteOff
};

// Build a time-ordered event list from the song + tempo.
std::vector<ScheduledEvent> build_schedule(const midi::Song &song,
                                           const midi::TempoMap &tempo) {
  std::vector<ScheduledEvent> evs;
  evs.reserve(song.notes.size());
  for (const auto &n : song.notes) {
    const double t = midi::ticks_to_seconds(n.tick, tempo);
    evs.push_back(
        ScheduledEvent{t, n.ch, n.note, n.vel, n.type == midi::EvType::NoteOn});
  }
  // Sort by time; at identical time, do NoteOff before NoteOn (avoids hanging
  // notes)
  std::sort(evs.begin(), evs.end(),
            [](const ScheduledEvent &a, const ScheduledEvent &b) {
              if (a.tSec != b.tSec)
                return a.tSec < b.tSec;
              if (a.on != b.on)
                return b.on; // false (Off) comes first
              if (a.ch != b.ch)
                return a.ch < b.ch;
              return a.note < b.note;
            });
  return evs;
}

// Shared playback state the audio thread uses.
struct PlaybackState {
  tsf *synth = nullptr;
  std::vector<ScheduledEvent> events;
  std::size_t nextIndex = 0; // next event to apply
  std::atomic<double> timeSec{0.0};
  double endTimeSec = 0.0;
  ma_uint32 sampleRate = 44100;
};

// Real-time callback: feed events up to t1, then render interleaved stereo s16.
void data_callback(ma_device *device, void *pOutput, const void * /*pInput*/,
                   ma_uint32 frameCount) {
  auto *st = reinterpret_cast<PlaybackState *>(device->pUserData);
  short *out = reinterpret_cast<short *>(pOutput);

  const double t0 = st->timeSec.load(std::memory_order_relaxed);
  const double dt =
      static_cast<double>(frameCount) / static_cast<double>(st->sampleRate);
  const double t1 = t0 + dt;

  // Apply all events that occur up to t1.
  while (st->nextIndex < st->events.size() &&
         st->events[st->nextIndex].tSec <= t1) {
    const auto &e = st->events[st->nextIndex++];
    if (e.on) {
      // vel 0..127 -> 0..1 gain
      tsf_note_on(st->synth, e.ch, e.note,
                  (e.vel <= 127 ? e.vel : 127) / 127.0f);
    } else {
      tsf_note_off(st->synth, e.ch, e.note);
    }
  }

  // Render audio for this buffer.
  // TinySoundFont renders "frames * channels" samples for interleaved stereo.
  tsf_render_short(st->synth, out, static_cast<int>(frameCount), 0);

  // Advance clock.
  st->timeSec.store(t1, std::memory_order_relaxed);

  // If we've passed the end + tail, we can fade quickly (optional, simple
  // ramp).
  if (t1 >= st->endTimeSec) {
    // simple post-tail fade: multiply buffer to zero over last buffer
    // (kept tiny; real implementations would smooth more carefully)
    const double tailLeft = std::max(0.0, st->endTimeSec - t0);
    double scale = tailLeft / dt; // 1..0 across this callback
    if (scale < 0.0)
      scale = 0.0;
    const int samples = static_cast<int>(frameCount) * 2; // stereo interleaved
    for (int i = 0; i < samples; ++i) {
      out[i] = static_cast<short>(out[i] * scale);
    }
  }
}

inline void ensure(bool cond, const char *msg) {
  if (!cond)
    throw std::runtime_error(msg);
}

} // namespace

namespace audio {

void play(const midi::Song &song, const midi::TempoMap &tempo,
          const std::filesystem::path &sf2Path) {
  // --- Build schedule & compute duration ---
  std::vector<ScheduledEvent> evs = build_schedule(song, tempo);
  double durationSec = 0.0;
  if (!evs.empty())
    durationSec = evs.back().tSec;
  const double tailSec = 2.0; // let reverb/decay ring out a moment

  // --- Init TinySoundFont ---
  tsf *synth = tsf_load_filename(sf2Path.string().c_str());
  ensure(synth != nullptr, "Failed to load SoundFont (.sf2)");

  const ma_uint32 sampleRate = 44100; // safe, common default
  tsf_set_output(synth, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate),
                 0.0f);
  tsf_set_volume(synth, 0.8f); // modest headroom

  // For now, set every channel to GM1 Acoustic Grand (program 0).
  // Later we can parse Program Change messages and set per-channel presets.
  for (int ch = 0; ch < 16; ++ch) {
    tsf_channel_set_presetnumber(synth, ch, 0 /*Acoustic Grand*/,
                                 true /*drums auto on ch10*/);
  }

  // --- Miniaudio device setup ---
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_s16; // matches tsf_render_short
  config.playback.channels = 2;           // stereo
  config.sampleRate = sampleRate;
  config.dataCallback = data_callback;

  PlaybackState state;
  state.synth = synth;
  state.events = std::move(evs);
  state.nextIndex = 0;
  state.timeSec = 0.0;
  state.endTimeSec = durationSec + tailSec;
  state.sampleRate = sampleRate;
  config.pUserData = &state;

  ma_device device;
  if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
    tsf_close(synth);
    throw std::runtime_error("Failed to open playback device");
  }

  // Start streaming.
  if (ma_device_start(&device) != MA_SUCCESS) {
    ma_device_uninit(&device);
    tsf_close(synth);
    throw std::runtime_error("Failed to start playback device");
  }

  // --- Block until done ---
  // We'll poll the audio-time clock; it advances only inside the callback.
  const auto start = std::chrono::steady_clock::now();
  while (state.timeSec.load(std::memory_order_relaxed) < state.endTimeSec) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    // Optional safety: break if wall clock is wildly longer than expected.
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (elapsed > state.endTimeSec + 10.0)
      break; // sanity escape
  }

  // Stop and clean up.
  ma_device_stop(&device);
  ma_device_uninit(&device);
  tsf_close(synth);
}

} // namespace audio
