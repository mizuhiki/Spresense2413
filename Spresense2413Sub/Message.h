#ifndef __MESSAGE_H__
#define __MESSAGE_H__

namespace Message {

enum Mesasge {
    kReqAudioRendering = 0,
    kSetRegValue,
};

struct SetRegValue_t {
    uint8_t addr;
    uint8_t value;
};

struct AudioRenderingBufferMsg_t {
    uint8_t *buff;
    size_t buffSize;
    SetRegValue_t *regValue;
    size_t regValueSize;
};

}

#endif /* __MESSAGE_H__ */
