/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Takashi Mizuhiki
 */

#if defined(ARDUINO_ARCH_SPRESENSE) && !defined(SUBCORE)

#include <MIDI.h>

#include "MidiInSrc.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

MidiInSrc::MidiInSrc(Filter& filter) : BaseFilter(filter) {
}

MidiInSrc::~MidiInSrc() {
}

bool MidiInSrc::setParam(int param_id, intptr_t value) {
    return BaseFilter::setParam(param_id, value);
}

intptr_t MidiInSrc::getParam(int param_id) {
    return BaseFilter::getParam(param_id);
}

bool MidiInSrc::isAvailable(int param_id) {
    return BaseFilter::isAvailable(param_id);
}

bool MidiInSrc::begin() {
    MIDI.begin(MIDI_CHANNEL_OMNI);
    printf("MidiInSrc::begin()\n");

    return BaseFilter::begin();
}

void MidiInSrc::update() {
    // process MIDI events
    if (MIDI.read()) {
        switch (MIDI.getType()) {
        case midi::NoteOn:
            sendNoteOn(MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
            break;

        case midi::NoteOff:
            sendNoteOff(MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
            break;

        default:
            break;
        }
    };

    BaseFilter::update();
}

#endif  // ARDUINO_ARCH_SPRESENSE
