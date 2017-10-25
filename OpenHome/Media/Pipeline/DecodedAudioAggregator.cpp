#include <OpenHome/Media/Pipeline/DecodedAudioAggregator.h>
#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Media/Debug.h>

using namespace OpenHome;
using namespace OpenHome::Media;

// DecodedAudioAggregator

const TUint DecodedAudioAggregator::kSupportedMsgTypes =   eMode
                                                         | eTrack
                                                         | eDrain
                                                         | eDelay
                                                         | eEncodedStream
                                                         | eMetatext
                                                         | eStreamInterrupted
                                                         | eHalt
                                                         | eFlush
                                                         | eWait
                                                         | eDecodedStream
                                                         | eBitRate
                                                         | eAudioPcm
                                                         | eAudioDsd
                                                         | eQuit;

DecodedAudioAggregator::DecodedAudioAggregator(IPipelineElementDownstream& aDownstreamElement)
    : PipelineElement(kSupportedMsgTypes)
    , iDownstreamElement(aDownstreamElement)
    , iDecodedAudio(nullptr)
    , iChannels(0)
    , iSampleRate(0)
    , iBitDepth(0)
    , iSupportsLatency(false)
    , iAggregationDisabled(false)
{
}

void DecodedAudioAggregator::Push(Msg* aMsg)
{
    ASSERT(aMsg != nullptr);
    Msg* msg = aMsg->Process(*this);
    if (msg != nullptr) {
        iDownstreamElement.Push(msg);
    }
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgMode* aMsg)
{
    OutputAggregatedAudio();
    iSupportsLatency = aMsg->Info().SupportsLatency();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgTrack* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgDrain* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgEncodedStream* aMsg)
{
    OutputAggregatedAudio();
    const auto wasAggregationDisabled = iAggregationDisabled;
    iAggregationDisabled = (iSupportsLatency && aMsg->RawPcm());
    if (wasAggregationDisabled != iAggregationDisabled) {
        LOG(kMedia, "DecodedAudioAggregator::ProcessMsg(MsgEncodedStream* ): iAggregationDisabled=%u\n",
                    iAggregationDisabled);
    }
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgStreamInterrupted* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgHalt* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgFlush* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgWait* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgDecodedStream* aMsg)
{
    OutputAggregatedAudio();
    ASSERT(iDecodedAudio == nullptr);
    const DecodedStreamInfo& info = aMsg->StreamInfo();
    iChannels = info.NumChannels();
    iSampleRate = info.SampleRate();
    iBitDepth = info.BitDepth();
    return aMsg;
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgAudioPcm* aMsg)
{
    return TryAggregate(aMsg);
}

Msg* DecodedAudioAggregator::ProcessMsg(MsgQuit* aMsg)
{
    OutputAggregatedAudio();
    return aMsg;
}

TBool DecodedAudioAggregator::AggregatorFull(TUint aBytes, TUint aJiffies)
{
    return (aBytes == DecodedAudio::kMaxBytes || aJiffies >= kMaxJiffies);
}

MsgAudioPcm* DecodedAudioAggregator::TryAggregate(MsgAudioPcm* aMsg)
{
    if (iAggregationDisabled) {
        return aMsg;
    }

    TUint jiffies = aMsg->Jiffies();
    const TUint jiffiesPerSample = Jiffies::PerSample(iSampleRate);
    const TUint msgBytes = Jiffies::ToBytes(jiffies, jiffiesPerSample, iChannels, iBitDepth);
    ASSERT(jiffies == aMsg->Jiffies()); // refuse to handle msgs not terminating on sample boundaries

    if (iDecodedAudio == nullptr) {
        if (AggregatorFull(msgBytes, aMsg->Jiffies())) {
            return aMsg;
        }
        else {
            iDecodedAudio = aMsg;
            return nullptr;
        }
    }

    TUint aggregatedJiffies = iDecodedAudio->Jiffies();
    TUint aggregatedBytes = Jiffies::ToBytes(aggregatedJiffies, jiffiesPerSample, iChannels, iBitDepth);
    if (aggregatedBytes + msgBytes <= kMaxBytes) {
        // Have byte capacity to add new data.
        iDecodedAudio->Aggregate(aMsg);

        aggregatedJiffies = iDecodedAudio->Jiffies();
        aggregatedBytes = Jiffies::ToBytes(aggregatedJiffies, jiffiesPerSample, iChannels, iBitDepth);
        if (AggregatorFull(aggregatedBytes, iDecodedAudio->Jiffies())) {
            MsgAudioPcm* msg = iDecodedAudio;
            iDecodedAudio = nullptr;
            return msg;
        }
    }
    else {
        // Lazy approach here - if new aMsg can't be appended, just return
        // iDecodedAudio and set iDecodedAudio = aMsg.
        // Could add a method to MsgAudioPcm that chops audio when aggregating
        // to make even more efficient use of decoded audio msgs.
        MsgAudioPcm* msg = iDecodedAudio;
        iDecodedAudio = aMsg;
        return msg;
    }

    return nullptr;
}

void DecodedAudioAggregator::OutputAggregatedAudio()
{
    if (iDecodedAudio != nullptr) {
        iDownstreamElement.Push(iDecodedAudio);
        iDecodedAudio = nullptr;
    }
}
