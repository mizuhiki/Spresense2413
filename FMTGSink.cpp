/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#define ARDUINO_ARCH_SPRESENSE

#if defined(ARDUINO_ARCH_SPRESENSE) && !defined(SUBCORE)

#include <MP.h>

#include <math.h>
#include "FMTGSink.h"

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

constexpr int kDefaultInstNo = 1; // Violin

static uint8_t buffer[FMTGSINK_SUB_CORE_NUM][kPbBlockSize];
static Message::AudioRenderingBufferMsg_t msg[FMTGSINK_SUB_CORE_NUM];

void FMTGSink::writeToRenderer(int ch)
{
    while (renderer_.getWritableSize(ch) >= kPbBlockSize) {
        size_t read_size = kPbBlockSize;

        MP.RecvTimeout(1000);

        // Recv ACK from sub core
        for (int core = 0; core < FMTGSINK_SUB_CORE_NUM; core++) {
            uint8_t rcvid;
            void *dataPtr;
            int ret = MP.Recv(&rcvid, &dataPtr, core + 1);
            if (ret < 0) {
                MPLog("MP Recv Error = %d, core = %d\n", ret, core + 1);
            }

            regValues_[core].clear();
        }

        // Mix all subcore output
        for (int core = 1; core < FMTGSINK_SUB_CORE_NUM; core++) {
            int16_t *d = (uint16_t *)buffer[0];
            int16_t *s = (uint16_t *)buffer[core];

            for (int i = 0; i < (read_size >> 1); i++) {
                *d += *s;
                d++; s++;
            }
        }

        // Send to audio driver
        renderer_.write(ch, buffer, read_size);

        // Send message to sub core
        for (int core = 0; core < FMTGSINK_SUB_CORE_NUM; core++) {
            regValues_[core] = regValuesStandby_[core];
            regValuesStandby_[core].clear();

            msg[core].buff = buffer[core];
            msg[core].buffSize = read_size;
            msg[core].regValue = regValues_[core].data();
            msg[core].regValueSize = regValues_[core].size();

            int ret = MP.Send(Message::kReqAudioRendering, &msg[core], core + 1);
            if (ret < 0) {
                MPLog("MP Send Error = %d, core = %d\n", ret, core + 1);
            }
        }
    }
}

int FMTGSink::getPlayingChannelMap(void)
{
    int map = 0;

    for (auto it = keyOnLog_.begin(); it != keyOnLog_.end(); ++it) {
        map |= 1 << (*it);
    }

#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    if (rhythmState_ & 0x1f) {
        map |= 1 << 6;
    }
#endif

    return map;
}

FMTGSink::FMTGSink() : NullFilter(),
    renderer_(kPbSampleFrq, kPbBitDepth, kPbChannelCount, kPbSampleCount, kPbCacheSize, 1),
    rhythmState_(0), isFirstRendering_(true) {
    for (int i = 0; i < FMTGSINK_MAX_VOICES; i++) {
        voices_[i].noteNo = INVALID_NOTE_NUMBER;
    }

    for (int ch = 0; ch < 16; ch++) {
        inst_[ch] = kDefaultInstNo;
    }

    memset(rhythmVolumes_, 0, sizeof(rhythmVolumes_));
}

FMTGSink::~FMTGSink() {
}

bool FMTGSink::begin() {
    // Boot sub cores
    for (int core = 0; core < FMTGSINK_SUB_CORE_NUM; core++) {
        int ret = MP.begin(core + 1);
        if (ret < 0) {
            MPLog("MP.begin(%d) error = %d\n", core + 1, ret);
        }
    }

    return true;
}

void FMTGSink::doFirstRendering() {
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

#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    int core = 2;
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x16, .value = 0x20 });
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x17, .value = 0x50 });
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x18, .value = 0xC0 });
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x26, .value = 0x05 });
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x27, .value = 0x05 });
    regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x28, .value = 0x01 });
#endif

    // Send message to sub core
    for (int core = 0; core < FMTGSINK_SUB_CORE_NUM; core++) {
        msg[core].buff = buffer[core];
        msg[core].buffSize = kPbBlockSize;
        msg[core].regValue = regValuesStandby_[core].data();
        msg[core].regValueSize = regValuesStandby_[core].size();

        int ret = MP.Send(Message::kReqAudioRendering, &msg[core], core + 1);
        if (ret < 0) {
            MPLog("MP Send Error = %d, core = %d\n", ret, core + 1);
        }
    }

    return true;
}

void FMTGSink::update() {
    if (isFirstRendering_) {
        isFirstRendering_ = false;
        doFirstRendering();
    }
            
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
    if (FMTGSink::PARAMID_INST <= param_id && param_id <= FMTGSink::PARAMID_INST + 15) {
        int ch = param_id - FMTGSink::PARAMID_INST;
        return inst_[ch];
    } else {
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
    }

    return NullFilter::getParam(param_id);
}

bool FMTGSink::setParam(int param_id, intptr_t value) {
    if (FMTGSink::PARAMID_INST <= param_id && param_id <= FMTGSink::PARAMID_INST + 15) {
        int ch = param_id - FMTGSink::PARAMID_INST;

        if (value < 0 || 15 < value) {
            return false;
        }

        inst_[ch] = value;

        return true;
    } else {
        switch (param_id) {
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

// https://github.com/keijiro/vst2413/blob/master/source/RhythmDriver.cpp
int NoteToKeyBit(int note) {
    switch (note) {
        case 36:
            return 16; // kick
        case 38:
            return 8; // snare
        case 43:
        case 47:
        case 50:
            return 4; // tom
        case 49:
        case 51:
            return 2; // cymbal
        case 42:
        case 44:
            return 1; // hat
    }
    return 0;
}

int NoteToVolumeRegisterPosition(int note) {
    switch (note) {
        case 36:
            return 0; // kick
        case 38:
            return 2; // snare
        case 43:
        case 47:
        case 50:
            return 5; // tom
        case 49:
        case 51:
            return 4; // cymbal
        case 42:
        case 44:
            return 3; // hat
    }
    return 1; // null
}

bool FMTGSink::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel > 15) {
        return false;
    }

#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    if (channel == 9) {
        rhythmState_ |= NoteToKeyBit(note);

        int vrp = NoteToVolumeRegisterPosition(note);
        rhythmVolumes_[vrp] = 0x0f - ((velocity >> 3) & 0x0f);
        uint8_t val = rhythmVolumes_[vrp | 1] << 4 | rhythmVolumes_[vrp & 6];
        //OPLL_writeReg(opll, 0x36 + (position >> 1), val);
        int core = 2;
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x36 + (vrp >> 1), .value = val });
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x0E, .value = 0x20 | (rhythmState_ & 0x1f) });
    } else {
#endif /* FMTGSINK_ENABLE_RHYTHM_MODE */
        // 空いているチャンネルを探す
        int ch = 0;
        int core = 0;
        for (ch = 0; ch < FMTGSINK_MAX_VOICES; ch++) {
            if (voices_[ch].noteNo == INVALID_NOTE_NUMBER) {
                break;
            }
        }

        core = ch / FMTGSINK_VOICES_PER_CORE;

        if (ch == FMTGSINK_MAX_VOICES) {
            // 空いているチャンネルがなかったため、一番古い KeyOn を乗っ取る（後着優先）
            ch = keyOnLog_[0];
            keyOnLog_.erase(keyOnLog_.begin());

            core = ch / FMTGSINK_VOICES_PER_CORE;
            regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x20 + ch, 0x00 });
        }

        voices_[ch].noteNo = note;
        voices_[ch].channel = channel;
        keyOnLog_.push_back(ch);

        int bf = CalculateBlockAndFNumber(note);
        uint8_t vol = 0x0f - ((velocity >> 3) & 0x0f);
        uint8_t inst = (uint8_t)inst_[channel] << 4;

        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x30 + ch, .value = inst | vol });
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x10 + ch, .value = bf & 0xff });
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x20 + ch, .value = 0x10 + (bf >> 8) });
#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    }
#endif /* FMTGSINK_ENABLE_RHYTHM_MODE */

    return true;
}

bool FMTGSink::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    if (channel == 9) {
        rhythmState_ &= ~NoteToKeyBit(note);
        int core = 2;
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x0E, .value = 0x20 | (rhythmState_ & 0x1f) });
    } else {
#endif /* FMTGSINK_ENABLE_RHYTHM_MODE */
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

        int core = ch / FMTGSINK_VOICES_PER_CORE;
        regValuesStandby_[core].push_back((Message::SetRegValue_t){ .addr = 0x20 + ch, .value = 0x00 });

        voices_[ch].noteNo = INVALID_NOTE_NUMBER;

        for (auto it = keyOnLog_.begin(); it != keyOnLog_.end(); ) {
            if (*it == ch) {
                it = keyOnLog_.erase(it);
            } else {
                ++it;
            }
        }
#ifdef FMTGSINK_ENABLE_RHYTHM_MODE
    }
#endif /* FMTGSINK_ENABLE_RHYTHM_MODE */

    return true;
}

#endif  // ARDUINO_ARCH_SPRESENSE
