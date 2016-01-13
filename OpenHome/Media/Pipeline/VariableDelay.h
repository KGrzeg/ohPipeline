#pragma once

#include <OpenHome/Types.h>
#include <OpenHome/Private/Standard.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Media/Pipeline/Msg.h>

namespace OpenHome {
namespace Media {

/*
Element which introduces a delay (likely for lip syncing)
If the delay is increased, silence is introduced.
If the delay is decreased, audio (pulled from upstream) is discarded.
Before any change in delay is actioned, audio spends RampDuration ramping down.
After a delay is actioned, audio spends RampDuration ramping up.
FIXME - no handling of pause-resumes
*/
    
class VariableDelay : private MsgReservoir, public IPipelineElementUpstream, private IMsgProcessor, private IStreamHandler
{
    static const TUint kMaxMsgSilenceDuration = Jiffies::kPerMs * 5;
    friend class SuiteVariableDelay;
public:
    VariableDelay(const TChar* aId, MsgFactory& aMsgFactory, IPipelineElementUpstream& aUpstreamElement, TUint aDownstreamDelay, TUint aRampDuration);
    virtual ~VariableDelay();
public: // from IPipelineElementUpstream
    Msg* Pull() override;
private:
    Msg* NextMsgLocked();
    MsgAudio* DoProcessAudioMsg(MsgAudio* aMsg);
    void RampMsg(MsgAudio* aMsg);
    void HandleStarving();
    void ResetStatusAndRamp();
private: // from IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg) override;
    Msg* ProcessMsg(MsgTrack* aMsg) override;
    Msg* ProcessMsg(MsgDrain* aMsg) override;
    Msg* ProcessMsg(MsgDelay* aMsg) override;
    Msg* ProcessMsg(MsgEncodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioEncoded* aMsg) override;
    Msg* ProcessMsg(MsgMetaText* aMsg) override;
    Msg* ProcessMsg(MsgStreamInterrupted* aMsg) override;
    Msg* ProcessMsg(MsgHalt* aMsg) override;
    Msg* ProcessMsg(MsgFlush* aMsg) override;
    Msg* ProcessMsg(MsgWait* aMsg) override;
    Msg* ProcessMsg(MsgDecodedStream* aMsg) override;
    Msg* ProcessMsg(MsgBitRate* aMsg) override;
    Msg* ProcessMsg(MsgAudioPcm* aMsg) override;
    Msg* ProcessMsg(MsgSilence* aMsg) override;
    Msg* ProcessMsg(MsgPlayable* aMsg) override;
    Msg* ProcessMsg(MsgQuit* aMsg) override;
private: // from IStreamHandler
    EStreamPlay OkToPlay(TUint aStreamId) override;
    TUint TrySeek(TUint aStreamId, TUint64 aOffset) override;
    TUint TryStop(TUint aStreamId) override;
    void NotifyStarving(const Brx& aMode, TUint aStreamId, TBool aStarving) override;
private:
    enum EStatus
    {
        EStarting
       ,ERunning
       ,ERampingDown
       ,ERampedDown
       ,ERampingUp
    };
private:
    const TChar* iId;
    MsgFactory& iMsgFactory;
    IPipelineElementUpstream& iUpstreamElement;
    TUint iDelayJiffies;
    Mutex iLock;
    TInt iDelayAdjustment;
    EStatus iStatus;
    Ramp::EDirection iRampDirection;
    const TUint iDownstreamDelay;
    const TUint iRampDuration;
    TBool iWaitForAudioBeforeGeneratingSilence;
    TUint iCurrentRampValue;
    TUint iRemainingRampSize;
    BwsMode iMode;
    IStreamHandler* iStreamHandler;
};

} // namespace Media
} // namespace OpenHome

