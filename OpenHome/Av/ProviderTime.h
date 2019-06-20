#pragma once

#include <Generated/DvAvOpenhomeOrgTime1.h>
#include <OpenHome/Net/Core/DvInvocationResponse.h>
#include <OpenHome/Media/PipelineManager.h>
#include <OpenHome/Media/PipelineObserver.h>
#include <OpenHome/Private/Thread.h>

namespace OpenHome {
namespace Av {

class ProviderTime : public Net::DvProviderAvOpenhomeOrgTime1, private Media::IPipelineObserver
{
public:
    ProviderTime(Net::DvDevice& aDevice, Media::PipelineManager& aPipelineManager);
private: // from DvProviderAvOpenhomeOrgTime1
    void Time(Net::IDvInvocation& aInvocation, Net::IDvInvocationResponseUint& aTrackCount, Net::IDvInvocationResponseUint& aDuration, Net::IDvInvocationResponseUint& aSeconds) override;
private: // from Media::IPipelineObserver
    void NotifyPipelineState(Media::EPipelineState aState) override;
    void NotifyMode(const Brx& aMode, const Media::ModeInfo& aInfo, const Media::ModeTransportControls& aTransportControls) override;
    void NotifyTrack(Media::Track& aTrack, TBool aStartOfStream) override;
    void NotifyMetaText(const Brx& aText) override;
    void NotifyTime(TUint aSeconds) override;
    void NotifyStreamInfo(const Media::DecodedStreamInfo& aStreamInfo) override;
private:
    Media::PipelineManager& iPipelineManager;
    Mutex iLock;
};

} // namespace Av
} // namespace OpenHome
