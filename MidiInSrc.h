/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#ifndef MIDIINSRC_H_
#define MIDIINSRC_H_

#include <time.h>

#include "ScoreReader.h"
#include "YuruInstrumentFilter.h"

class MidiInSrc : public BaseFilter {
public:
    MidiInSrc(Filter& filter);
    ~MidiInSrc();

    bool setParam(int param_id, intptr_t value) override;
    intptr_t getParam(int param_id) override;
    bool isAvailable(int param_id) override;
    bool begin() override;
    void update() override;
};

#endif  // MIDIINSRC_H_
