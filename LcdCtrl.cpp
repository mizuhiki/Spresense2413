/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#include "LcdCtrl.h"
#include "FMTGSink.h"

#define TFT_DC 9
#define TFT_CS -1
#define TFT_RST 8

static Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

void LcdCtrl::Setup(void)
{
    tft.begin(40000000);
    tft.setRotation(3);
}

void LcdCtrl::ShowLoadingScreen(void)
{
  tft.fillScreen(ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);

  tft.setCursor(5, 5);
  tft.println("Spresense2413");

  tft.setCursor(5, 35);
  tft.println("Now loading...");
}

void LcdCtrl::ShowChStatusScreen(void)
{
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(3);
  tft.setCursor(265, 35);
  tft.println("Ok");

  tft.setCursor(5, 95);
  tft.println("[Ch. Status]");

  tft.setCursor(5, 125);
  tft.println("1 2 3 4 5 6 R");
}

void LcdCtrl::UpdateChStatus(int ch_map)
{
    int channels = FMTGSINK_MAX_VOICES;

#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    channels++;
#endif

    for (int ch = 0; ch < channels; ch++) {
        if (ch_map & (1 << ch)) {
            tft.fillRect(ch * 36 + 2, 150, 20, 20, ILI9341_WHITE);
        } else {
            tft.fillRect(ch * 36 + 2, 150, 20, 20, ILI9341_NAVY);
        }
    }
}

