/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#ifndef ARDUINO_ARCH_SPRESENSE
#error "Board selection is wrong!!"
#endif
#ifdef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <MemoryUtil.h>
#include <MP.h>

#include "FMTGSink.h"
#include "MidiInSrc.h"

#include "LcdCtrl.h"

const char *inst_name[] = {
    "User",              // 0
    "Violin",            // 1
    "Guitar",            // 2
    "Piano",             // 3
    "Flute",             // 4
    "Clarinet",          // 5
    "Oboe",              // 6
    "Trumpet",           // 7
    "Organ",             // 8
    "Horn",              // 9
    "Synthesizer",       // 10
    "Harpsichord",       // 11
    "Vibraphone",        // 12
    "Synthesizer Bass",  // 13
    "Acoustic Bass",     // 14
    "Electric Guitar"    // 15
};

#define INIT_INST_NO 1 // Violin
#define SPRESENSE_LED_NUM 4

FMTGSink fmTGSink;
MidiInSrc inst(fmTGSink);
LcdCtrl lcdCtrl;

int button4 = HIGH;
int button5 = HIGH;

static constexpr int InitInstNo[] = {
    1,  // MIDI ch.1 (Violin)
    2,  // MIDI ch.2 (Guitar)
    3,  // MIDI ch.3 (Piano)
    4,  // MIDI ch.4 (Flute)
    5,  // MIDI ch.5 (Clarinet)
    6,  // MIDI ch.6 (Oboe)
    7,  // MIDI ch.7 (Trumpet)
    8,  // MIDI ch.8 (Organ)
    9,  // MIDI ch.9 (Horn)
    0,  // MIDI Ch.10 (Rhythm Voice)
    10, // MIDI ch.11 (Synthesizer)
    11, // MIDI ch.12 (Harpsichord)
    12, // MIDI ch.13 (Vibraphone)
    13, // MIDI ch.14 (Synthesizer Bass)
    14, // MIDI ch.15 (Acoustic Bass)
    15, // MIDI Ch.16 (Electric Guitar)
};

static void showCurrentInst(void)
{
    int instNo = inst.getParam(FMTGSink::PARAMID_INST);
    printf("Inst #%d: %s\n", instNo, inst_name[instNo]);
}

void setup() {
    // init built-in I/O
    Serial.begin(115200);
    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);

    lcdCtrl.Setup();
    lcdCtrl.ShowLoadingScreen();

    // init buttons
    pinMode(PIN_D04, INPUT_PULLUP);
    pinMode(PIN_D05, INPUT_PULLUP);

    // initialize memory pool
    initMemoryPools();
    createStaticPools(MEM_LAYOUT_RECORDINGPLAYER);

    // setup instrument
    if (!inst.begin()) {
        Serial.println("ERROR: init error.");
        while (true) {
            delay(1000);
        }
    }

    // 初期音色の設定
    for (int ch = 0; ch < 16; ch++) {
        inst.setParam(FMTGSink::PARAMID_INST + ch, InitInstNo[ch]);
    }

    lcdCtrl.ShowChStatusScreen();

    Serial.println("Ready to play Spresense EMU2413.");
    Serial.println("[Button D4] Change instrument / [Button D5] Play A4(440Hz)");
    showCurrentInst();
}

void loop() {
    // Change instrument
    int button4_input = digitalRead(PIN_D04);
    if (button4_input != button4) {
        delay(10); // dechattering
        if (digitalRead(PIN_D04) == button4_input) {
            if (button4_input == LOW) {
                int instNo = inst.getParam(FMTGSink::PARAMID_INST);

                instNo++;
                if (instNo > 15) {
                    instNo = 0;
                }

                for (int ch = 0; ch < 16; ch++) {
                    // 全てのチャンネルの音色を同じ音色に設定する
                    inst.setParam(FMTGSink::PARAMID_INST + ch, instNo);
                }

                showCurrentInst();
            }

            button4 = button4_input;
        }
    }

    // Play A4 (440Hz)
    int button5_input = digitalRead(PIN_D05);
    if (button5_input != button5) {
        delay(10);
        if (digitalRead(PIN_D05) == button5_input) {
            if (button5_input == LOW) {
                inst.sendNoteOn(69, DEFAULT_VELOCITY, DEFAULT_CHANNEL);
            } else {
                inst.sendNoteOff(69, DEFAULT_VELOCITY, DEFAULT_CHANNEL);
            }
            button5 = button5_input;
        }
    }

    // run instrument
    inst.update();

    unsigned long time = millis();
    if (time % 10 == 0) {
        // Light the LED according to the playback channel
        int map = inst.getParam(FMTGSink::PARAMID_PLAYING_CH_MAP);
        for (int ch = 0; ch < std::min(FMTGSINK_MAX_VOICES, SPRESENSE_LED_NUM); ch++) {
            if (map & (1 << ch)) {
                digitalWrite(LED0 + ch, HIGH);
            } else {
                digitalWrite(LED0 + ch, LOW);
            }
        }

        lcdCtrl.UpdateChStatus(map);
    }
}
