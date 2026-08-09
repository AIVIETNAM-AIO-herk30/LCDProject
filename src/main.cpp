#include <Arduino.h>
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <vector>
#include <string>
#include "LGFX_Config.hpp"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include <WiFi.h>

#define SWITCH_INTERVAL_MS  (1UL * 60UL * 1000UL)
#define BRIGHTNESS          180
#define FADE_STEP           5
#define FADE_DELAY_MS       8
#define BTN_PIN             9 
#define DEBOUNCE_MS         200

void GIFDraw(GIFDRAW *pDraw);
void *GIFOpenFile(const char *fname, int32_t *pSize);
void  GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);

LGFX                tft;
lgfx::LGFX_Sprite   canvas(&tft);
AnimatedGIF         gif;
File                gifFile;
int                 xoff = 0, yoff = 0;

std::vector<std::string> gifList;
int           gifIndex   = 0;
bool          gifOpen    = false;
bool          displayOn  = true;
unsigned long gifStartMs = 0;
unsigned long lastBtnMs  = 0;

void collectGifs() {
  gifList.clear();
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    std::string name = "/" + std::string(f.name());
    if (name.size() > 4) {
      std::string ext = name.substr(name.size() - 4);
      for (char &c : ext) c = tolower(c);
      if (ext == ".gif") {
        gifList.push_back(name);
        Serial.printf("  GIF: %s (%u bytes)\n", name.c_str(), (unsigned)f.size());
      }
    }
    f = root.openNextFile();
  }
  Serial.printf("Tim thay %u file GIF\n", (unsigned)gifList.size());
}

void fadeTo(int target) {
  int b = tft.getBrightness();
  int step = (target > b) ? FADE_STEP : -FADE_STEP;
  for (; (step > 0) ? (b < target) : (b > target); b += step) {
    tft.setBrightness(b);
    delay(FADE_DELAY_MS);
  }
  tft.setBrightness(target);
}

void openGif(int idx) {
  if (gifOpen) { gif.close(); gifOpen = false; }

  canvas.fillSprite(TFT_BLACK);
  canvas.pushSprite(0, 0);

  const char *path = gifList[idx].c_str();
  Serial.printf(">> %s\n", path);

  if (gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    xoff = (240 - gif.getCanvasWidth())  / 2;
    yoff = (240 - gif.getCanvasHeight()) / 2;
    gifOpen    = true;
    gifStartMs = millis();
  } else {
    Serial.printf("Loi mo GIF (err %d)\n", gif.getLastError());
  }
}

void GIFDraw(GIFDRAW *pDraw) {
  static uint16_t usTemp[240];
  uint16_t *usPalette = pDraw->pPalette;
  int y = pDraw->iY + pDraw->y;
  int iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > 240) iWidth = 240 - pDraw->iX;
  if (iWidth < 1) return;

  uint8_t *s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (int x = 0; x < iWidth; x++)
      if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    uint8_t tr = pDraw->ucTransparent;
    int xo = 0;
    while (xo < iWidth) {
      int c = 0;
      while (xo + c < iWidth && s[xo + c] != tr) { usTemp[c] = usPalette[s[xo + c]]; c++; }
      if (c) { canvas.pushImage(pDraw->iX + xo + xoff, y + yoff, c, 1, usTemp); xo += c; }
      while (xo < iWidth && s[xo] == tr) xo++;
    }
  } else {
    for (int x = 0; x < iWidth; x++) usTemp[x] = usPalette[s[x]];
    canvas.pushImage(pDraw->iX + xoff, y + yoff, iWidth, 1, usTemp);
  }
}

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (!gifFile) return NULL;
  *pSize = gifFile.size();
  return (void *)&gifFile;
}
void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f) f->close();
}
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  File *f = static_cast<File *>(pFile->fHandle);
  int32_t n = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen) n = pFile->iSize - pFile->iPos - 1;
  if (n <= 0) return 0;
  n = (int32_t)f->read(pBuf, n);
  pFile->iPos = f->position();
  return n;
}
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

void setup() {
  setCpuFrequencyMhz(80);
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  pinMode(BTN_PIN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.setBrightness(BRIGHTNESS);
  tft.fillScreen(TFT_BLACK);

  canvas.setColorDepth(16);
  if (!canvas.createSprite(240, 240)) {
    Serial.printf("Khong du RAM! heap: %u\n", ESP.getFreeHeap());
    tft.setTextColor(TFT_RED); tft.drawString("Low RAM", 80, 110);
    while (true) delay(1000);
  }
  canvas.fillSprite(TFT_BLACK);

  if (!LittleFS.begin(true)) {
    tft.setTextColor(TFT_RED); tft.drawString("No FS", 90, 110);
    while (true) delay(1000);
  }

  collectGifs();
  if (gifList.empty()) {
    tft.setTextColor(TFT_YELLOW); tft.drawString("No GIF", 80, 110);
    while (true) delay(1000);
  }

  gif.begin(BIG_ENDIAN_PIXELS);
  openGif(0);
}

void loop() {
  // Nut BOOT (GPIO9): nhan de tat man hinh va ngu light sleep
  if (digitalRead(BTN_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastBtnMs > DEBOUNCE_MS) {
      lastBtnMs = now;

      // Tat man hinh
      fadeTo(0);

      // Cau hinh GPIO9 lam wakeup source (nhan nut = keo xuong LOW)
      gpio_wakeup_enable(GPIO_NUM_9, GPIO_INTR_LOW_LEVEL);
      esp_sleep_enable_gpio_wakeup();

      // Ngu light sleep — CPU dung, RAM giu nguyen, ~1mA
      esp_light_sleep_start();

      // Thuc day sau khi nhan nut lan nua
      delay(DEBOUNCE_MS);  // cho nut nhả ra
      lastBtnMs = millis();
      fadeTo(BRIGHTNESS);
    }
  }

  if (!gifOpen) return;

  if (millis() - gifStartMs >= SWITCH_INTERVAL_MS) {
    gifIndex = (gifIndex + 1) % gifList.size();
    openGif(gifIndex);
    return;
  }

  int frameDelay = 0;
  int result = gif.playFrame(false, &frameDelay);
  canvas.pushSprite(0, 0);
  if (result == 0) gif.reset();
  if (frameDelay > 1) delay(frameDelay);
}
