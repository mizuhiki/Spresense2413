/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#ifndef FMTGSINKSUB_H_
#define FMTGSINKSUB_H_

#include <stdint.h>
//#include <vector>

extern "C" {
#include "emu2413.h"
}

#define FMTGSINK_MAX_VOICES 3

class FMTGSinkSub {
private:
    OPLL *opll_;

public:
    FMTGSinkSub();
    ~FMTGSinkSub();

    bool begin();
    void renderToBuffer(uint8_t *buff, size_t size);
    void setRegValue(uint8_t addr, uint8_t value);
};

#endif  // FMTGSINKSUB_H_
