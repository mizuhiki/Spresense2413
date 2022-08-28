/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#ifndef ARDUINO_ARCH_SPRESENSE
#error "Board selection is wrong!!"
#endif
#ifndef SUBCORE
#error "Core selection is wrong!!"
#endif

#define ARDUINO_ARCH_SPRESENSE
#if defined(ARDUINO_ARCH_SPRESENSE)

#include <math.h>
#include "FMTGSinkSub.h"

// playback parameters
const int kPbSampleFrq = 48000;
const int kPbBitDepth = 16;
const int kPbChannelCount = 2;
const int kPbSampleCount = 240;
const int kPbBlockSize = kPbSampleCount * (kPbBitDepth / 8) * kPbChannelCount;
const int kPbCacheSize = (2 * 1024);

const int kPbBytePerSec = kPbSampleFrq * (kPbBitDepth / 8) * kPbChannelCount;

const int kVolumeMin = -1020;
const int kVolumeMax = 120;

// cache parameter
const int kPreloadFrameNum = 3;
const int kLoadFrameNum = 10;

// OPLL parameter
constexpr int kOpllClk = 3456000; // = 48000 * 72
constexpr int kFreqA4 = 440; // Hz
constexpr int kFnumA4 = kFreqA4 * 262144 /* = 2^18 */ / kPbSampleFrq / 16 /* = 2^oct */;

FMTGSinkSub::FMTGSinkSub() {
}

FMTGSinkSub::~FMTGSinkSub() {
}

bool FMTGSinkSub::begin() {
    bool ok = true;

    opll_ = OPLL_new(kOpllClk, kPbSampleFrq);
    OPLL_setVoiceNum(opll_, FMTGSINK_MAX_VOICES);

    int enable_ch = 0;

#if (SUBCORE == 1)
    for (int i = 0; i < 3; i++) {
        enable_ch |= 1 << i;
    }
#elif (SUBCORE == 2)
    for (int i = 3; i < 6; i++) {
        enable_ch |= 1 << i;
    }
#elif (SUBCORE == 3)
    for (int i = 6; i < 9; i++) {
        enable_ch |= 1 << i;
    }
    enable_ch |= OPLL_MASK_RHYTHM;
#endif

    OPLL_setMask(opll_, ~enable_ch);

    return ok;
}

void FMTGSinkSub::renderToBuffer(uint8_t *buff, size_t size)
{
    uint8_t *p = buff;

    for (int i = 0; i < (size >> 2); i++) {
        int16_t out = OPLL_calcNoRateConv(opll_);
        uint8_t out_L =  out & 0xff;
        uint8_t out_H = (out & 0xff00) >> 8;

        *p++ = out_L; // Lch
        *p++ = out_H;
        *p++ = out_L; // Rch
        *p++ = out_H;
    }
}

void FMTGSinkSub::setRegValue(uint8_t addr, uint8_t value)
{
    OPLL_writeReg(opll_, addr, value);
}

#endif  // ARDUINO_ARCH_SPRESENSE
