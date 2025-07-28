#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>
#include "I2SOutput.h"
#include "PlaylistSampleSource.h"

i2s_pin_config_t i2sPins = {
    .bck_io_num = GPIO_NUM_27,
    .ws_io_num = GPIO_NUM_14,
    .data_out_num = GPIO_NUM_26,
    .data_in_num = -1};

I2SOutput *output = nullptr;
PlaylistSampleSource *playlist = nullptr;

bool countingDown = false;
int countdownFrom = -1;
unsigned long lastPlayTime = 0;
int currentNumber = -1;

std::vector<const char *> getNumberAudioFiles(int number) {
  std::vector<const char *> files;

  if (number == 0) {
    files.push_back("/0.wav");
    return files;
  }

  int hundreds = number / 100;
  int remainder = number % 100;

  if (hundreds > 0) {
    char *hundred_digit = (char *)malloc(12);
    snprintf(hundred_digit, 12, "/%d.wav", hundreds);
    files.push_back(hundred_digit);
    files.push_back(strdup("/100.wav"));
  }

  if (remainder > 0) {
    if (remainder <= 20 || remainder % 10 == 0) {
      char *path = (char *)malloc(12);
      snprintf(path, 12, "/%d.wav", remainder);
      files.push_back(path);
    } else {
      int tens = (remainder / 10) * 10;
      int units = remainder % 10;

      char *tens_path = (char *)malloc(12);
      snprintf(tens_path, 12, "/%d.wav", tens);
      files.push_back(tens_path);

      char *units_path = (char *)malloc(12);
      snprintf(units_path, 12, "/%d.wav", units);
      files.push_back(units_path);
    }
  }

  return files;
}

void playNumber(int number) {
  std::vector<const char *> files = getNumberAudioFiles(number);

  for (auto &file : files) {
    Serial.print("Queued: ");
    Serial.println(file);
  }

  // Stop and delete previous output and playlist
  if (output) {
    output->stop();     // <--- Important: stop I2S and kill task
    delete output;
    output = nullptr;
  }

  if (playlist) {
    delete playlist;
    playlist = nullptr;
  }

  // Create new playlist and output
  playlist = new PlaylistSampleSource(files);
  output = new I2SOutput();
  output->start(I2S_NUM_1, i2sPins, playlist);

  
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  Serial.println("SPIFFS Ready");
  Serial.println("Enter a number (0–999) to start countdown:");
}

void loop() {
  // Read input from Serial Monitor
  if (Serial.available() > 0 && !countingDown) {
    String input = Serial.readStringUntil('\n');
    countdownFrom = input.toInt();
    if (countdownFrom >= 0 && countdownFrom <= 999) {
      countingDown = true;
      currentNumber = countdownFrom;
      lastPlayTime = millis() - 1500; // allow immediate start
      Serial.print("Starting countdown from ");
      Serial.println(countdownFrom);
    } else {
      Serial.println("Invalid number. Enter a value from 0–999.");
    }
  }

  // Perform countdown
  if (countingDown && millis() - lastPlayTime >= 2000) { // ~1.5s per count
    playNumber(currentNumber);
    lastPlayTime = millis();
    currentNumber--;

    if (currentNumber < 0) {
      countingDown = false;
      Serial.println("Countdown complete. Enter a new number:");
    }
  }
}
