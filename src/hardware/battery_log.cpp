#include "hardware/battery_log.h"

#include <SD_MMC.h>

#include <algorithm>

#include "hardware/sd_storage.h"

namespace {
constexpr char runFolder[] = "/battery";
constexpr uint8_t maxFilesScanned = 32;

// "run_12.csv" (with or without a folder in front) -> 12; 0 if the name doesn't match
uint16_t runNumberFromName(const char *name) {
  const char *slash = strrchr(name, '/');
  if (slash) name = slash + 1;
  unsigned number = 0;
  char extension[8] = "";
  if (sscanf(name, "run_%u.%7s", &number, extension) == 2 && strcmp(extension, "csv") == 0) return number;
  return 0;
}

// Run numbers in the folder, smallest (oldest) first
uint8_t listRunNumbers(uint16_t *numbers, uint8_t capacity) {
  File folder = SD_MMC.open(runFolder);
  if (!folder || !folder.isDirectory()) return 0;
  uint8_t count = 0;
  for (File file = folder.openNextFile(); file && count < capacity; file = folder.openNextFile()) {
    uint16_t number = file.isDirectory() ? 0 : runNumberFromName(file.name());
    if (number) numbers[count++] = number;
  }
  std::sort(numbers, numbers + count);
  return count;
}

// Reads in 512-byte blocks rather than a character at a time, so a run of thousands of lines loads quickly
void forEachLine(File &file, const std::function<void(const char *)> &onLine) {
  uint8_t buffer[512];
  char line[64];
  size_t length = 0;
  for (;;) {
    int got = file.read(buffer, sizeof(buffer));
    if (got <= 0) break;
    for (int i = 0; i < got; ++i) {
      char c = buffer[i];
      if (c == '\n' || c == '\r') {
        if (length) {
          line[length] = 0;
          onLine(line);
          length = 0;
        }
      } else if (length < sizeof(line) - 1) {
        line[length++] = c;
      }
    }
  }
  if (length) {
    line[length] = 0;
    onLine(line);
  }
}

// Cell from the first line; duration and STOP from the last few lines, so the whole file isn't read
bool readRunSummary(uint16_t number, BatteryRun &run) {
  File file = SD_MMC.open(batteryRunPath(number), FILE_READ);
  if (!file) return false;
  run = {number, 0, 0, false};
  String firstLine = file.readStringUntil('\n');
  int cellAt = firstLine.indexOf("cell ");
  if (cellAt >= 0) run.cell = firstLine.substring(cellAt + 5).toInt();
  size_t size = file.size();
  if (size > 96) file.seek(size - 96);
  forEachLine(file, [&run](const char *line) {
    unsigned long seconds = 0;
    float voltage = 0;
    if (strcmp(line, "# stopped") == 0) {
      run.stopped = true;
    } else if (sscanf(line, "%lu,%f", &seconds, &voltage) == 2) {
      run.seconds = seconds;
    }
  });
  file.close();
  return true;
}

bool appendLine(uint16_t number, const String &line) {
  File file = SD_MMC.open(batteryRunPath(number), FILE_APPEND);
  if (!file) return false;
  bool written = file.print(line) == line.length();
  file.close();
  return written;
}
}  // namespace

String batteryRunPath(uint16_t number) {
  return String(runFolder) + "/run_" + String(number) + ".csv";
}

uint8_t listBatteryRuns(BatteryRun *runs, uint8_t capacity) {
  if (!sdCardReady()) return 0;
  uint16_t numbers[maxFilesScanned];
  uint8_t count = listRunNumbers(numbers, maxFilesScanned);
  uint8_t first = count > capacity ? count - capacity : 0;  // keep the newest if there are too many
  uint8_t listed = 0;
  for (uint8_t i = first; i < count; ++i) {
    if (readRunSummary(numbers[i], runs[listed])) ++listed;
  }
  return listed;
}

bool readBatteryRun(uint16_t number, const std::function<void(uint32_t seconds, float voltage)> &onSample) {
  File file = SD_MMC.open(batteryRunPath(number), FILE_READ);
  if (!file) return false;
  forEachLine(file, [&onSample](const char *line) {
    unsigned long seconds = 0;
    float voltage = 0;
    if (sscanf(line, "%lu,%f", &seconds, &voltage) == 2) onSample(seconds, voltage);
  });
  file.close();
  return true;
}

uint16_t startBatteryRun(uint8_t cell) {
  if (!sdCardReady()) return 0;
  if (!SD_MMC.exists(runFolder)) SD_MMC.mkdir(runFolder);
  uint16_t numbers[maxFilesScanned];
  uint8_t count = listRunNumbers(numbers, maxFilesScanned);
  uint16_t number = count ? numbers[count - 1] + 1 : 1;
  for (uint8_t oldest = 0; count - oldest >= maxBatteryRuns; ++oldest) {
    SD_MMC.remove(batteryRunPath(numbers[oldest]));
    Serial.printf("Battery run %u deleted to make room\n", numbers[oldest]);
  }
  File file = SD_MMC.open(batteryRunPath(number), FILE_WRITE);
  if (!file) return 0;
  file.printf("# ES3C40P battery run %u, cell %u\nelapsed_s,voltage_v\n", number, cell);
  file.close();
  Serial.printf("Battery run %u started (cell %u)\n", number, cell);
  return number;
}

bool logBatteryRunSample(uint16_t number, uint32_t seconds, float voltage) {
  return appendLine(number, String(seconds) + "," + String(voltage, 3) + "\n");
}

void stopBatteryRun(uint16_t number) {
  appendLine(number, "# stopped\n");
  Serial.printf("Battery run %u stopped\n", number);
}
