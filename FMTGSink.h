/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#ifndef FMTGSINK_H_
#define FMTGSINK_H_

#include <stdint.h>

#include <vector>

#include <Arduino.h>

#include <File.h>

#include "PcmRenderer.h"
#include "WavReader.h"
#include "YuruInstrumentFilter.h"

extern "C" {
#include "emu2413.h"
}

#define FMTGSINK_MAX_VOICES 3

class FMTGSink : public NullFilter {
private:
    OPLL *opll_;

    int volume_;
    int inst_;
    PcmRenderer renderer_;

    struct Voice {
        int noteNo;
        int channel;
    };
    
    Voice voices_[FMTGSINK_MAX_VOICES];
    std::vector<int> keyOnLog_;

    void writeToRenderer(int ch);
    int getPlayingChannelMap(void);

public:
    enum ParamId {                         // MAGIC CHAR = 'F'
        PARAMID_INST           = ('F' << 8),  //<
        PARAMID_PLAYING_CH_MAP
    };

    // Constructor
    FMTGSink();
    ~FMTGSink();

    bool isAvailable(int param_id) override;
    intptr_t getParam(int param_id) override;
    bool setParam(int param_id, intptr_t value) override;

    bool begin() override;
    void update() override;

    bool sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) override;
    bool sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) override;
};

#endif  // FMTGSINK_H_
