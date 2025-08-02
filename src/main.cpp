#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>
#include "I2SOutput.h"
#include "PlaylistSampleSource.h"

// I²S pin configuration
i2s_pin_config_t i2sPins = {
  .bck_io_num   = GPIO_NUM_27,
  .ws_io_num    = GPIO_NUM_14,
  .data_out_num = GPIO_NUM_26,
  .data_in_num  = -1
};

I2SOutput*             audio    = nullptr;
PlaylistSampleSource*  playlist = nullptr;

// Hold WAV filenames as Strings (so buffers are owned)…
std::vector<String>    fileNames;
// …then once built, grab their c_str() pointers here:
std::vector<const char*> filePtrs;

// Build and validate the full countdown list from 'start' down to 0
void buildCountdownList(int start) {
  fileNames.clear();
  // Rough upper bound: each number might produce up to 2 files
  fileNames.reserve((start + 1) * 2);

  auto tryEnqueue = [&](const String& path) {
    if (SPIFFS.exists(path)) {
      fileNames.push_back(path);
    } else {
      Serial.printf("Missing file: %s\n", path.c_str());
    }
  };

  for (int n = start; n >= 0; --n) {
    if (n == 0) {
      tryEnqueue("/0.wav");
    } else {
      int hundreds  = n / 100;
      int remainder = n % 100;

      if (hundreds > 0) {
        tryEnqueue("/" + String(hundreds) + ".wav");
        tryEnqueue("/100.wav");
      }

      if (remainder > 0) {
        if (remainder <= 20 || remainder % 10 == 0) {
          tryEnqueue("/" + String(remainder) + ".wav");
        } else {
          int tens  = (remainder / 10) * 10;
          int units = remainder % 10;
          tryEnqueue("/" + String(tens) + ".wav");
          tryEnqueue("/" + String(units) + ".wav");
        }
      }
    }
  }

  // Now snapshot pointers (after all Strings are in place)
  filePtrs.clear();
  filePtrs.reserve(fileNames.size());
  for (auto& s : fileNames) {
    filePtrs.push_back(s.c_str());
  }

  Serial.printf("Built playlist: %u files, heap=%u\n",
                (unsigned)filePtrs.size(), ESP.getFreeHeap());
}

void startFullCountdown(int start) {
  // 1) Clean up any previous run
  if (audio) {
    audio->stop();
    delete audio;
    audio = nullptr;
  }
  if (playlist) {
    delete playlist;
    playlist = nullptr;
  }

  // 2) Build the playlist
  buildCountdownList(start);
  if (filePtrs.empty()) {
    Serial.println("⛔ No valid WAV files found—cannot start countdown.");
    return;
  }

  // 3) Create & start
  playlist = new PlaylistSampleSource(filePtrs);
  audio    = new I2SOutput();
  audio->start(I2S_NUM_1, i2sPins, playlist);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed—halting.");
    while (true) delay(1000);
  }
  Serial.println("SPIFFS Ready. Enter 0–999 to countdown:");
}

void loop() {
  static bool running   = false;
  static unsigned long doneAt = 0;

  // — New countdown?
  if (!running && Serial.available()) {
    int input = Serial.readStringUntil('\n').toInt();
    if (input >= 0 && input <= 999) {
      Serial.printf("▶️ Starting countdown from %d\n", input);
      startFullCountdown(input);
      // Estimate ~1.8s per number, so we know when to stop.
      doneAt = millis() + (unsigned long)(input + 1) * 1800;
      running = true;
    } else {
      Serial.println("Invalid—enter a number from 0 to 999.");
    }
  }

  // — Detect finish
  if (running && audio && millis() >= doneAt) {
    audio->stop();
    delete audio;    audio    = nullptr;
    delete playlist; playlist = nullptr;

    Serial.println("✅ Countdown complete. Enter new number:");
    running = false;
    doneAt   = 0;
  }
}
