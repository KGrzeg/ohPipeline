#pragma once

#include <OpenHome/Types.h>
#include <OpenHome/Private/Standard.h>
#include <OpenHome/Private/Network.h>

EXCEPTION(OhmTimestampNotFound);

namespace OpenHome {
    class Environment;
namespace Av {

class IOhmTimestamper
{
public:
    virtual ~IOhmTimestamper() {}
    virtual void Start(const Endpoint& aDst) = 0;
    virtual void Stop() = 0;
    virtual TUint Timestamp(TUint aFrame) = 0;
    virtual TBool SetSampleRate(TUint aSampleRate) = 0;
};

} // namespace Av
} // namespace OpenHome
