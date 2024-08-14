#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Arduino_JSON.h>

#include <ctime>

#include "ICameraConfig.hpp"

//pin configurations
static constexpr gpio_num_t PIN_PIR = GPIO_NUM_19;
static constexpr int PIN_LED = 14;
static constexpr int PIN_MIC_CLK = 47;
static constexpr int PIN_MIC_DAT = 48;
static constexpr int PIN_SD_CS = 9;
static constexpr int PIN_SD_MOSI = 38;
static constexpr int PIN_SD_CLK = 39;
static constexpr int PIN_SD_MISO = 40;

//other constants
static constexpr char IMAGE_DIR[] = "/Images";
static constexpr uint64_t ONE_SEC = 1000000ULL;
static constexpr uint32_t PIR_MARGIN = 10;  //sec
static constexpr size_t MAX_PATH = 64;
static constexpr int FLASH_SHORT = 75;
static constexpr int FLASH_LONG = 500;

enum class Mode : uint8_t {
  Timer,
  PIR,
  Both,
};

enum class CaptureType : char {
  Timer = 'T',
  PIR = 'P',
};
using CT = CaptureType;

enum class ErrorCode {
  OK,
  ErrSdBegin,
  ErrMkdir,
  ErrInitCam,
  ErrFbGet,
  ErrFileOpen,
};
using EC = ErrorCode;

//local variables
static SPIClass* s_pSD = nullptr;
static UnitCamS3Config s_camera;

static RTC_DATA_ATTR Mode s_eMode = Mode::Timer;
static RTC_DATA_ATTR uint64_t s_interval = 0ULL;  //intarval for timelapse(us)
static RTC_DATA_ATTR uint32_t s_timDirNo = 0U;    //folder# for timelapse
static RTC_DATA_ATTR uint32_t s_timFileIdx = 1U;  //file# for timelapse
static RTC_DATA_ATTR time_t s_timPrevCap = 0;     //previous capture time of timelapse
static RTC_DATA_ATTR uint32_t s_pirDirNo = 0U;    //folder# for PIR
static RTC_DATA_ATTR uint32_t s_pirFileIdx = 1U;  //file# for PIR
static RTC_DATA_ATTR time_t s_pirPrevCap = 0;     //previous capture time of PIR

//convert sec to microsec
static inline uint64_t sec2us(time_t sec) {
  return (static_cast<uint64_t>(sec) * ONE_SEC);
}

static inline time_t getUnixTime() {
  return std::time(nullptr);
}

static inline void turnLED(bool isON) {
  digitalWrite(PIN_LED, (isON ? LOW : HIGH));
}

static void flashLED(std::initializer_list<int> list) {
  for (auto t : list) {
    turnLED(true);
    delay(t);
    turnLED(false);
    delay(100);
  }
  delay(1000);
}

static void showError(ErrorCode eCode) {
  switch (eCode) {
    case EC::ErrSdBegin:
      for (;;) {
        flashLED({ FLASH_SHORT });
      }
      break;
    case EC::ErrMkdir:
      for (;;) {
        flashLED({ FLASH_SHORT, FLASH_SHORT });
      }
      break;
    case EC::ErrInitCam:
      for (;;) {
        flashLED({ FLASH_SHORT, FLASH_SHORT, FLASH_SHORT });
      }
      break;
    case EC::ErrFbGet:
      for (;;) {
        flashLED({ FLASH_LONG });
      }
      break;
    case EC::ErrFileOpen:
      for (;;) {
        flashLED({ FLASH_SHORT, FLASH_LONG });
      }
      break;
    default:
      for (;;) {
        flashLED({ FLASH_SHORT, FLASH_SHORT, FLASH_LONG });
      }
      break;
  }
}

static inline bool isPIR(CaptureType eType) {
  return (eType == CT::PIR);
}

static inline uint32_t getDirNo(CaptureType eType) {
  return isPIR(eType) ? s_pirDirNo : s_timDirNo;
}

static inline void setDirNo(CaptureType eType, uint32_t dirNo) {
  if (isPIR(eType)) {
    s_pirDirNo = dirNo;
  } else {
    s_timDirNo = dirNo;
  }
}

static inline uint32_t getFileIdx(CaptureType eType) {
  return isPIR(eType) ? s_pirFileIdx : s_timFileIdx;
}

static inline void setFileIdx(CaptureType eType, uint32_t idx) {
  if (isPIR(eType)) {
    s_pirFileIdx = idx;
  } else {
    s_timFileIdx = idx;
  }
}

static inline uint32_t nextFileIdx(CaptureType eType) {
  uint32_t idx = getFileIdx(eType);
  setFileIdx(eType, (idx + 1U));
  return idx;
}

static inline uint32_t getPrevCap(CaptureType eType) {
  return isPIR(eType) ? s_pirPrevCap : s_timPrevCap;
}

static inline void setPrevCap(CaptureType eType) {
  if (isPIR(eType)) {
    s_pirPrevCap = getUnixTime();
  } else {
    s_timPrevCap = getUnixTime();
  }
}

static inline uint64_t getInterval(void) {
  return s_interval;
}

static inline void setInterval(uint64_t interval) {
  s_interval = interval;
}

static inline Mode getMode(void) {
  return s_eMode;
}

static inline void setMode(Mode eMode) {
  s_eMode = eMode;
}

static inline void makePath(char pPath[MAX_PATH], CaptureType eType, uint32_t dirNo) {
  sprintf(pPath, "%s/%C%07lu", IMAGE_DIR, (char)eType, dirNo);
}

static void makeNewDir(CaptureType eType) {
  auto dirNo = getDirNo(eType);
  char pPath[MAX_PATH]{};
  do {
    makePath(pPath, eType, ++dirNo);
  } while (SD.exists(pPath));

  if (SD.mkdir(pPath) == 0) {
    showError(EC::ErrMkdir);
  }
  setDirNo(eType, dirNo);
}

static void initSD(void) {
  s_pSD = new SPIClass(HSPI);
  s_pSD->begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  constexpr uint8_t pPinMap[] = {
    PIN_SD_CLK,
    PIN_SD_MISO,
    PIN_SD_MOSI,
  };
  for (auto& pin : pPinMap) {
    *(volatile uint32_t*)(GPIO_PIN_MUX_REG[pin]) |= FUN_DRV_M;
    gpio_pulldown_dis((gpio_num_t)pin);
    gpio_pullup_en((gpio_num_t)pin);
  }

  if (SD.begin(PIN_SD_CS, *s_pSD, 10000000) == false) {
    showError(EC::ErrSdBegin);
  }

  //if 'Images' folder isn't there, create it.
  if (SD.exists(IMAGE_DIR) == false) {
    SD.mkdir(IMAGE_DIR);
  }
}

//getting interval from JSON
static void parseJson(void) {
  constexpr char JSONFILE[] = "/Pref.json";
  constexpr char UNDEFINED[] = "undefined";
  constexpr char INTERVAL[] = "interval";
  constexpr char TYPE[] = "type";

  //default value
  setMode(Mode::Timer);
  setInterval(sec2us(60));  //1min

  if (SD.exists(JSONFILE)) {
    auto file = SD.open(JSONFILE, FILE_READ);
    if (file) {
      const auto len = file.size();
      if (len > 0U) {
        auto bytes = new uint8_t[len + 1];
        if (file.read(bytes, len) == len) {
          bytes[len] = '\0';
          auto objects = JSON.parse(reinterpret_cast<char*>(bytes));
          if (JSON.typeof(objects) == UNDEFINED) {
            //NOP
          } else {
            if (objects.hasOwnProperty(INTERVAL)) {
              const auto value = static_cast<time_t>((int)objects[INTERVAL]);
              setInterval(sec2us(value));
            }
            if (objects.hasOwnProperty(TYPE)) {
              auto pType = (const char*)objects[TYPE];
              USBSerial.printf("value:%s\n", pType);
              if (strcmp(pType, "pir") == 0) {
                setMode(Mode::PIR);
              } else if (strcmp(pType, "both") == 0) {
                setMode(Mode::Both);
              } else {
                setMode(Mode::Timer);
              }
            }
          }
        }
        delete[] bytes;
      }
      file.close();
    }
  }
}

static void capture(CaptureType eType, bool is1stCapture = true) {
  if (s_camera.Initialize() == false) {
    showError(EC::ErrInitCam);
  }

  turnLED(true);
  if (is1stCapture) {
    //drop several frames cuz WB is unstable
    for (int32_t i = 0; i < 15; i++) {
      auto pFB = esp_camera_fb_get();
      if (pFB == nullptr) {
        showError(EC::ErrFbGet);
      }
      esp_camera_fb_return(pFB);
    }
  }

  auto pFB = esp_camera_fb_get();
  if (pFB == nullptr) {
    showError(EC::ErrFbGet);
  }

  char pFile[16]{};
  sprintf(pFile, "/%04lu.jpg", nextFileIdx(eType));

  char pPath[MAX_PATH]{};
  const auto dirNo = getDirNo(eType);
  makePath(pPath, eType, getDirNo(eType));
  strcat(pPath, pFile);

  auto file = SD.open(pPath, FILE_WRITE);
  if (file == false) {
    showError(EC::ErrFileOpen);
  }

  file.write(pFB->buf, pFB->len);
  file.close();
  esp_camera_fb_return(pFB);
  setPrevCap(eType);
  turnLED(false);

  if (getFileIdx(eType) > 9999) {
    setFileIdx(eType, 1U);
    makeNewDir(eType);
  }
}

static uint64_t onWakeup1st(void) {
  parseJson();
  switch (getMode()) {
    case Mode::Timer:
      makeNewDir(CT::Timer);
      break;
    case Mode::PIR:
      makeNewDir(CT::PIR);
      break;
    case Mode::Both:
      makeNewDir(CT::Timer);
      makeNewDir(CT::PIR);
      break;
  }
  setPrevCap(CT::Timer);
  return getInterval();
}

static uint64_t onWakeupByTimer(time_t tStart) {
  capture(CT::Timer);

  //reduce interval from 'full-duration' cuz capture wastes its time
  auto wkUpInterval = getInterval();
  const auto tmp = sec2us(getPrevCap(CT::Timer) - tStart);
  if (tmp >= wkUpInterval) {
    wkUpInterval = 1ULL;
  } else {
    wkUpInterval -= tmp;
  }
  return wkUpInterval;
}

static uint64_t onWakeupByPIR(time_t tStart) {
  //captures again if it already spent PIR_MARGIN from previous PIR capturing
  if ((getUnixTime() - getPrevCap(CT::PIR)) > PIR_MARGIN) {
    capture(CT::PIR);
  }

  for (;;) {
    //wait PIR goes to LOW
    if (digitalRead(PIN_PIR) == LOW) {
      break;
    }
    //break if PIR holds HIGH over PIR_MARGIN
    if ((getUnixTime() - getPrevCap(CT::PIR)) > PIR_MARGIN) {
      break;
    }
    delay(50);
  }

  auto wkUpInterval = getInterval();
  if (getMode() == Mode::Both) {
    //reduce sleep time when the mode is 'both', cuz it's waking up in timelapse
    auto tmp = sec2us(getUnixTime() - getPrevCap(CT::Timer));
    if (tmp >= wkUpInterval) {
      //capture before into sleep if it already overs next interval timing
      capture(CT::Timer, false);
      //reduce interval from 'full' by the time it took capture
      tmp = sec2us(getPrevCap(CT::Timer) - tStart);
    }
    if (tmp < wkUpInterval) {
      wkUpInterval -= tmp;
    }
  }

  return wkUpInterval;
}

void setup() {
  const auto tStart = getUnixTime();

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PIR, INPUT);
  turnLED(false);

  initSD();

  //switch the behavior by trigger
  uint64_t wkUpInterval = 0ULL;
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      wkUpInterval = onWakeupByPIR(tStart);
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      wkUpInterval = onWakeupByTimer(tStart);
      break;
    default:
      wkUpInterval = onWakeup1st();
      break;
  }

  //decide the trigger
  switch (getMode()) {
    case Mode::PIR:
      esp_sleep_enable_ext0_wakeup(PIN_PIR, HIGH);
      break;
    case Mode::Both:
      esp_sleep_enable_ext0_wakeup(PIN_PIR, HIGH);
      esp_sleep_enable_timer_wakeup(wkUpInterval);
      break;
    default:
      esp_sleep_enable_timer_wakeup(wkUpInterval);
      break;
  }

  //enter sleep
  esp_deep_sleep_start();
}

void loop() {
  delay(1000);
}
