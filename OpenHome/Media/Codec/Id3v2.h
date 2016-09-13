#pragma once

#include <OpenHome/Types.h>
#include <OpenHome/Media/Codec/Container.h>

namespace OpenHome {
namespace Media {
namespace Codec {

class Id3v2 : public ContainerBase
{
private:
    static const TUint kRecogniseBytes = 10;
public:
    Id3v2();
public: // from ContainerBase
    Msg* Recognise() override;
    TBool Recognised() const override;
    void Reset() override;
    void Init(TUint64 aStreamBytes) override;
    TBool TrySeek(TUint aStreamId, TUint64 aOffset) override;
    Msg* Pull() override;
private:
    TBool RecogniseTag();
private:
    enum EState {
        eNone,
        eRecognising,
        ePulling,
    };
private:
    TBool iRecognitionStarted;
    TBool iRecognitionSuccess;
    EState iState;
    TUint iSize;
    TUint iTotalSize;
    Bws<kRecogniseBytes> iBuf;
};

} // namespace Codec
} // namespace Media
} // namespace OpenHome

