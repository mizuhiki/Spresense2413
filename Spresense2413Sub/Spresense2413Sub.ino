// subcore

#include <MP.h>

#include "FMTGSinkSub.h"
#include "Message.h"

FMTGSinkSub fmTGSink;

void setup()
{
    fmTGSink.begin();
    MP.begin();
}

void loop()
{
    int8_t msgid;
    void *msg;

    int ret = MP.Recv(&msgid, &msg);
    if (ret < 0) {
        MPLog("MP Recv Error = %d\n", ret);
    }

    switch (msgid) {
    case Message::kReqAudioRendering:
        {
            auto* m = static_cast<Message::AudioRenderingBufferMsg_t *>(msg);
            for (int i = 0; i < m->regValueSize; i++) {
                auto* regValue = &m->regValue[i];
                fmTGSink.setRegValue(regValue->addr, regValue->value);
            }

            fmTGSink.renderToBuffer(m->buff, m->buffSize);
        }
        break;

    default:
        break;
    }

    // reply ACK
    ret = MP.Send(msgid, (void *)NULL);
    if (ret < 0) {
        MPLog("MP Send Error = %d\n", ret);
    }
}

