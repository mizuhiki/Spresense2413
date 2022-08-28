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

#include "PcmRenderer.h"
#include "YuruInstrumentFilter.h"

#include "Spresense2413Sub/Message.h"

#define FMTGSINK_MAX_VOICES 6
#define FMTGSINK_SUB_CORE_NUM 3
#define FMTGSINK_VOICES_PER_CORE 3
#define FMTGSINK_ENABLE_RHYTHM_MODE

class FMTGSink : public NullFilter {
private:
    int volume_;
    int inst_[16];  // 各チャンネルに設定した音色番号
    PcmRenderer renderer_;
    bool isFirstRendering_;

    struct Voice {
        int noteNo;
        int channel;
    };
    
    Voice voices_[FMTGSINK_MAX_VOICES];
    std::vector<int> keyOnLog_;
    std::vector<Message::SetRegValue_t> regValuesStandby_[FMTGSINK_SUB_CORE_NUM];
    std::vector<Message::SetRegValue_t> regValues_[FMTGSINK_SUB_CORE_NUM];
    uint8_t rhythmState_;
    uint8_t rhythmVolumes_[6];
    
    void doFirstRendering();
    void writeToRenderer(int ch);
    int getPlayingChannelMap(void);

public:
    enum ParamId {                             // MAGIC CHAR = 'F'
        PARAMID_INST            = ('F' << 8),  //<
        PARAMID_PLAYING_CH_MAP  = PARAMID_INST + 16
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
