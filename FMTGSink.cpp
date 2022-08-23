/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#if defined(ARDUINO_ARCH_SPRESENSE) && !defined(SUBCORE)

#include <math.h>
#include "FMTGSink.h"

#include <SDHCI.h>

// playback parameters
const int kPbSampleFrq = 48000;
const int kPbBitDepth = 16;
const int kPbChannelCount = 2;
const int kPbSamoleCount = 240;
const int kPbBlockSize = kPbSamoleCount * (kPbBitDepth / 8) * kPbChannelCount;
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


static uint32_t msToSa(int ms) {
    return kPbSampleFrq * ms / 1000 * (kPbBitDepth / 8) * kPbChannelCount;
}

static uint32_t msToByte(int ms) {
    return kPbBytePerSec * ms / 1000;
}

void FMTGSink::writeToRenderer(int ch)
{
    while (renderer_.getWritableSize(ch) >= kPbBlockSize) {
        size_t read_size = renderer_.getWritableSize(ch);
        if (read_size > kPbBlockSize) {
            read_size = kPbBlockSize;
        }

        uint8_t buffer[read_size];
        uint8_t *p = buffer;

        for (int i = 0; i < (read_size >> 2); i++) {
            int16_t out = OPLL_calcNoRateConv(opll_);
            uint8_t out_L =  out & 0xff;
            uint8_t out_H = (out & 0xff00) >> 8;

            *p++ = out_L; // Lch
            *p++ = out_H;
            *p++ = out_L; // Rch
            *p++ = out_H;
        }

        renderer_.write(ch, buffer, read_size);
    }
}

int FMTGSink::getPlayingChannelMap(void)
{
    int map = 0;

    for (auto it = keyOnLog_.begin(); it != keyOnLog_.end(); ++it) {
        map |= 1 << (*it);
    }

    return map;
}

FMTGSink::FMTGSink() : NullFilter(),
    renderer_(kPbSampleFrq, kPbBitDepth, kPbChannelCount, kPbSamoleCount, kPbCacheSize, 1), inst_(1) {
    for (int i = 0; i < FMTGSINK_MAX_VOICES; i++) {
        voices_[i].noteNo = INVALID_NOTE_NUMBER;
    }
}

FMTGSink::~FMTGSink() {
}

bool FMTGSink::begin() {
    bool ok = true;

    opll_ = OPLL_new(kOpllClk, kPbSampleFrq);
    OPLL_setVoiceNum(opll_, FMTGSINK_MAX_VOICES);

    int enable_ch = 0;
    for (int i = 0; i < FMTGSINK_MAX_VOICES; i++) {
        enable_ch |= 1 << i;
    }
    OPLL_setMask(opll_, ~enable_ch);

    // setup renderer
    renderer_.begin();
    renderer_.clear(0);

    // preload sound
    for (int i = 0; i < kPreloadFrameNum; i++) {
        if (!renderer_.render()) {
            break;
        }
    }

    renderer_.setState(PcmRenderer::kStateActive);

    return ok;
}

void FMTGSink::update() {
    switch (renderer_.getState()) {
        case PcmRenderer::kStateReady:
            break;
        case PcmRenderer::kStateActive:
            writeToRenderer(0);
            break;
        case PcmRenderer::kStatePause:
            break;
        default:
            break;
    }
}

bool FMTGSink::isAvailable(int param_id) {
    switch (param_id) {
    case FMTGSink::PARAMID_INST:
        return true;

    case FMTGSink::PARAMID_PLAYING_CH_MAP:
        return true;

    case Filter::PARAMID_OUTPUT_LEVEL:
        return true;

    default:
        break;
    }

    return NullFilter::isAvailable(param_id);
}

intptr_t FMTGSink::getParam(int param_id) {
    switch (param_id) {
    case FMTGSink::PARAMID_INST:
        return inst_;

    case FMTGSink::PARAMID_PLAYING_CH_MAP:
        return getPlayingChannelMap();

    case Filter::PARAMID_OUTPUT_LEVEL:
        return volume_;
    
    default:
        break;
    }

    return NullFilter::getParam(param_id);
}

bool FMTGSink::setParam(int param_id, intptr_t value) {
    switch (param_id) {
    case FMTGSink::PARAMID_INST:
        if (inst_ < 0 || 15 < inst_) {
            return false;
        }
        inst_ = value;
        return true;

    case FMTGSink::PARAMID_PLAYING_CH_MAP:
        // read-only
        break;

    case Filter::PARAMID_OUTPUT_LEVEL:
        volume_ = constrain(value, kVolumeMin, kVolumeMax);
        renderer_.setVolume(volume_, 0, 0);
        return true;
    
    default:
        break;
    }

    return NullFilter::setParam(param_id, value);
}


// F-number calculation come from SynthDriver.cpp in keijiro/vst2413
// https://github.com/keijiro/vst2413/blob/master/source/SynthDriver.cpp

static int CalculateFNumber(int note, float tune) {
    int intervalFromA = (note - 9) % 12;
    return kFnumA4 * powf(2.0f, (1.0f / 12) * (intervalFromA + tune));
}

static int Clamp(int value, int min, int max)
{
    return value < min ? min : (value > max ? max : value);
}

static int NoteToBlock(int note) {
    return Clamp((note - 9) / 12, 0, 7);
}

static int CalculateBlockAndFNumber(int note) {
    return (NoteToBlock(note) << 9) + CalculateFNumber(note, 0);
}

bool FMTGSink::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    // 空いているチャンネルを探す
    int ch = 0;
    for (ch = 0; ch < FMTGSINK_MAX_VOICES; ch++) {
        if (voices_[ch].noteNo == INVALID_NOTE_NUMBER) {
            break;
        }
    }

    if (ch == FMTGSINK_MAX_VOICES) {
        // 空いているチャンネルがなかったため、一番古い KeyOn を乗っ取る（後着優先）
        ch = keyOnLog_[0];
        keyOnLog_.erase(keyOnLog_.begin());

        OPLL_writeReg(opll_, 0x20 + ch, 0x00); // keyoff
    }

    voices_[ch].noteNo = note;
    voices_[ch].channel = channel;
    keyOnLog_.push_back(ch);

    if (NOTE_NUMBER_MIN <= note && note <= NOTE_NUMBER_MAX) {
        int bf = CalculateBlockAndFNumber(note);
        uint8_t vol = 0x0f - (velocity >> 3) & 0x0f;
        uint8_t inst = (uint8_t)inst_ << 4;
        OPLL_writeReg(opll_, 0x30 + ch, inst | vol);       /* set inst# and volume. */
        OPLL_writeReg(opll_, 0x10 + ch, bf & 0xFF);        /* set F-Number(L). */
        OPLL_writeReg(opll_, 0x20 + ch, 0x10 + (bf >> 8)); /* set BLK & F-Number(H) and keyon. */
    }

    return true;
}

bool FMTGSink::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    // Note Off すべきチャンネルをスキャン
    int ch = 0;
    for (ch = 0; ch < FMTGSINK_MAX_VOICES; ch++) {
        if (voices_[ch].noteNo == note && voices_[ch].channel == channel) {
            break;
        }
    }

    if (ch == FMTGSINK_MAX_VOICES) {
        // Note Off すべきチャンネルが見つからなかった
        return false;
    }

    OPLL_writeReg(opll_, 0x20 + ch, 0x00); // keyoff

    voices_[ch].noteNo = INVALID_NOTE_NUMBER;

    for (auto it = keyOnLog_.begin(); it != keyOnLog_.end(); ) {
        if (*it == ch) {
            it = keyOnLog_.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

#endif  // ARDUINO_ARCH_SPRESENSE
