#include <OpenHome/Media/Pipeline/Pruner.h>
#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Media/Pipeline/Msg.h>

using namespace OpenHome;
using namespace OpenHome::Media;

// Pruner

const TUint Pruner::kSupportedMsgTypes =   eMode
                                         | eTrack
                                         | eDrain
                                         | eDelay
                                         | eMetatext
                                         | eStreamInterrupted
                                         | eHalt
                                         | eFlush
                                         | eWait
                                         | eDecodedStream
                                         | eBitRate
                                         | eAudioPcm
                                         | eSilence
                                         | eQuit;

Pruner::Pruner(IPipelineElementUpstream& aUpstreamElement)
    : PipelineElement(kSupportedMsgTypes)
    , iUpstreamElement(aUpstreamElement)
    , iPendingMode(nullptr)
    , iWaitingForAudio(false)
    , iConsumeHalts(false)
{
}

Pruner::~Pruner()
{
    if (iPendingMode != nullptr) {
        iPendingMode->RemoveRef();
    }
}

Msg* Pruner::Pull()
{
    Msg* msg = nullptr;
    do {
        if (iWaitingForAudio || iQueue.IsEmpty()) {
            msg = iUpstreamElement.Pull();
            msg = msg->Process(*this);
        }
        else if (iPendingMode != nullptr) {
            msg = iPendingMode;
            iPendingMode = nullptr;
        }
        else {
            msg = iQueue.Dequeue();
        }
    } while (msg == nullptr);
    return msg;
}

Msg* Pruner::TryQueue(Msg* aMsg)
{
    if (iWaitingForAudio) {
        iQueue.Enqueue(aMsg);
        return nullptr;
    }
    return aMsg;
}

Msg* Pruner::TryQueueCancelWaiting(Msg* aMsg)
{
    Msg* msg = TryQueue(aMsg);
    iWaitingForAudio = false;
    return msg;
}

Msg* Pruner::ProcessMsg(MsgMode* aMsg)
{
    if (iWaitingForAudio) {
        iQueue.Clear();
    }
    iWaitingForAudio = true;
    if (iPendingMode != nullptr) {
        iPendingMode->RemoveRef();
    }
    iPendingMode = aMsg;
    return nullptr;
}

Msg* Pruner::ProcessMsg(MsgTrack* aMsg)
{
    aMsg->RemoveRef();
    return nullptr;
}

Msg* Pruner::ProcessMsg(MsgMetaText* aMsg)
{
    aMsg->RemoveRef();
    return nullptr;
}

Msg* Pruner::ProcessMsg(MsgHalt* aMsg)
{
    // if we've passed on a Halt more recently than any audio, there's no need to pass on another Halt
    if (iConsumeHalts) {
        aMsg->RemoveRef();
        return nullptr;
    }
    iConsumeHalts = true;
    return TryQueueCancelWaiting(aMsg);
}

Msg* Pruner::ProcessMsg(MsgFlush* aMsg)
{
    return TryQueueCancelWaiting(aMsg);
}

Msg* Pruner::ProcessMsg(MsgWait* aMsg)
{
    aMsg->RemoveRef();
    return nullptr;
}

Msg* Pruner::ProcessMsg(MsgDecodedStream* aMsg)
{
    if (iWaitingForAudio) {
        /* The last track contains no audio data.  Discard any queued msgs rather than risk
           them adding to an ever-growing queue in a downstream component which buffers audio (StarvationMonitor) */
        iQueue.Clear();
    }
    iWaitingForAudio = true;
    return TryQueue(aMsg);
}

Msg* Pruner::ProcessMsg(MsgBitRate* aMsg)
{
    aMsg->RemoveRef();
    return nullptr;
}

Msg* Pruner::ProcessMsg(MsgAudioPcm* aMsg)
{
    iConsumeHalts = false;
    return TryQueueCancelWaiting(aMsg);
}

Msg* Pruner::ProcessMsg(MsgSilence* aMsg)
{
    return TryQueueCancelWaiting(aMsg);
}

Msg* Pruner::ProcessMsg(MsgQuit* aMsg)
{
    return TryQueueCancelWaiting(aMsg);
}
