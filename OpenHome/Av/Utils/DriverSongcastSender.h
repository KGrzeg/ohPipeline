#pragma once

#include <OpenHome/Types.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Media/Utils/ProcessorAudioUtils.h>
#include <OpenHome/Net/Core/DvDevice.h>
#include <OpenHome/Av/Songcast/OhmSender.h>

namespace OpenHome {
    class Environment;
    class Timer;
namespace Av {
    class ZoneHandler;
}
namespace Av {

class DriverSongcastSender : public Media::PipelineElement, private Net::IResourceManager
{
    static const TUint kSongcastTtl = 1;
    static const TUint kSongcastLatencyMs = 300;
    static const TUint kSongcastPreset = 0;
    static const Brn kSenderIconFileName;
    static const TUint kSupportedMsgTypes;
public:
    DriverSongcastSender(Media::IPipelineElementUpstream& aPipeline, TUint aMaxMsgSizeJiffies, Net::DvStack& aDvStack, const Brx& aName, TUint aChannel);
    ~DriverSongcastSender();
private:
    void DriverThread();
    void TimerCallback();
    void SendAudio(Media::MsgPlayable* aMsg);
    void DeviceDisabled();
private: // from Media::IMsgProcessor
    Media::Msg* ProcessMsg(Media::MsgMode* aMsg) override;
    Media::Msg* ProcessMsg(Media::MsgDrain* aMsg) override;
    Media::Msg* ProcessMsg(Media::MsgHalt* aMsg) override;
    Media::Msg* ProcessMsg(Media::MsgDecodedStream* aMsg) override;
    Media::Msg* ProcessMsg(Media::MsgPlayable* aMsg) override;
    Media::Msg* ProcessMsg(Media::MsgQuit* aMsg) override;
private: // from Net::IResourceManager
    void WriteResource(const Brx& aUriTail, const TIpAddress& aInterface, std::vector<char*>& aLanguageList, Net::IResourceWriter& aResourceWriter) override;
private:
    Media::IPipelineElementUpstream& iPipeline;
    TUint iMaxMsgSizeJiffies;
    Environment& iEnv;
    OhmSenderDriver* iOhmSenderDriver;
    OhmSender* iOhmSender;
    Net::DvDeviceStandard* iDevice;
    ZoneHandler* iZoneHandler;
    ThreadFunctor *iThread;
    Semaphore iDeviceDisabled;
    Timer* iTimer;
    TUint iSampleRate;
    TUint iJiffiesPerSample;
    TUint iNumChannels;
    TUint iBitDepth;
    TUint iJiffiesToSend;
    TUint iTimerFrequencyMs;
    TUint64 iLastTimeUs;    // last time stamp from system
    TInt  iTimeOffsetUs;    // running offset in usec from ideal time
                            //  <0 means sender is behind
                            //  >0 means sender is ahead
    Media::MsgPlayable* iPlayable;
    TBool iAudioSent;
    TBool iQuit;
};

} // namespace Av
} // namespace OpenHome

