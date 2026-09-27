// Desktop simulator: renders messages with the firmware's own layout and
// playback code and writes every frame to a file, for tools/preview.py to
// turn into an animated GIF.
//
//   pio run -e sim
//   .pio/build/sim/program --out keys.frames '{"text": "Keys away first! :key:", "effect": "flash"}'
//
// Output: "LBSIM1 <width> <height>\n", then per frame a little-endian uint32
// timestamp in microseconds followed by width*height RGB triplets (row-major,
// top-left first, before gamma and brightness: what a person perceives).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "lb/Canvas.h"
#include "lb/HomeAssistant.h"
#include "lb/Message.h"
#include "lb/Player.h"
#include "lb/Version.h"

namespace {

constexpr uint32_t kMaxSimUs = 900000000;  // 15 minutes

void usage() {
  std::fprintf(stderr,
               "usage: program [--width N] [--height N] [--speed PX_PER_S] [--duration S]\n"
               "               [--brightness PCT] --out FILE PAYLOAD [PAYLOAD...]\n"
               "       program --discovery   (print the Home Assistant discovery payload)\n"
               "PAYLOAD is message JSON or plain text, exactly as Home Assistant would publish it.\n");
}

// The discovery payload of a fully-equipped sign, for tools/validate_ha.py.
int printDiscovery() {
  lb::Topics topics("lite-brite", "entryway");
  lb::DeviceInfo info;
  info.name = "Entryway sign";
  info.model = "LED sign 32x8";
  info.hardware = "simulator";
  info.version = lb::kFirmwareVersion;
  info.supportUrl = lb::kProjectUrl;
  info.usbSense = true;
  std::printf("%s\n", lb::discoveryPayload(topics, info).c_str());
  return 0;
}

bool readInt(const char* text, int lo, int hi, int& out) {
  char* end = nullptr;
  const long v = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' || v < lo || v > hi) return false;
  out = static_cast<int>(v);
  return true;
}

void writeU32(std::FILE* f, uint32_t v) {
  const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16),
                        static_cast<uint8_t>(v >> 24)};
  std::fwrite(b, 1, sizeof(b), f);
}

}  // namespace

int main(int argc, char** argv) {
  int width = 32;
  int height = 8;
  lb::PlayDefaults defaults;
  const char* outPath = nullptr;
  std::vector<std::string> payloads;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool hasValue = i + 1 < argc;
    int v = 0;
    if (arg == "--width" && hasValue && readInt(argv[++i], 1, 512, v)) {
      width = v;
    } else if (arg == "--height" && hasValue && readInt(argv[++i], 1, 128, v)) {
      height = v;
    } else if (arg == "--speed" && hasValue && readInt(argv[++i], 1, 500, v)) {
      defaults.speed = static_cast<uint16_t>(v);
    } else if (arg == "--duration" && hasValue && readInt(argv[++i], 1, 600, v)) {
      defaults.durationS = static_cast<uint16_t>(v);
    } else if (arg == "--brightness" && hasValue && readInt(argv[++i], 1, 100, v)) {
      defaults.brightness = static_cast<uint8_t>(v);
    } else if (arg == "--out" && hasValue) {
      outPath = argv[++i];
    } else if (arg == "--discovery") {
      return printDiscovery();
    } else if (arg == "--help" || arg == "-h") {
      usage();
      return 0;
    } else if (arg.rfind("--", 0) == 0) {
      std::fprintf(stderr, "bad option: %s\n", arg.c_str());
      usage();
      return 2;
    } else {
      payloads.push_back(arg);
    }
  }
  if (outPath == nullptr || payloads.empty()) {
    usage();
    return 2;
  }

  lb::Playlist playlist;
  playlist.configure(defaults, width, height);
  for (size_t i = 0; i < payloads.size(); ++i) {
    lb::Message msg;
    std::string error;
    if (!lb::parseMessage("sim" + std::to_string(i), payloads[i], msg, error)) {
      std::fprintf(stderr, "payload %zu: %s\n", i + 1, error.c_str());
      return 2;
    }
    msg.seq = static_cast<uint32_t>(i + 1);
    playlist.add(msg);
  }

  std::FILE* out = std::fopen(outPath, "wb");
  if (out == nullptr) {
    std::perror(outPath);
    return 1;
  }
  std::fprintf(out, "LBSIM1 %d %d\n", width, height);
  lb::Canvas canvas(width, height);
  std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
  uint32_t t = 0;
  size_t frames = 0;
  while (playlist.active() && t < kMaxSimUs) {
    lb::Message finished;
    playlist.update(canvas, t, &finished);
    for (size_t p = 0; p < canvas.size(); ++p) {
      rgb[p * 3] = canvas.data()[p].r;
      rgb[p * 3 + 1] = canvas.data()[p].g;
      rgb[p * 3 + 2] = canvas.data()[p].b;
    }
    writeU32(out, t);
    std::fwrite(rgb.data(), 1, rgb.size(), out);
    ++frames;
    t += playlist.frameIntervalUs();
  }
  std::fclose(out);
  std::fprintf(stderr, "%zu frames, %.1f s\n", frames, t / 1e6);
  return 0;
}
