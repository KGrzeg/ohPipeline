#pragma once

#include <OpenHome/Av/Credentials.h>
#include <OpenHome/Types.h>
#include <OpenHome/Configuration/ConfigManager.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Http.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Net/Private/DviStack.h>
#include <OpenHome/Av/MediaPlayer.h>
#include <Generated/CpAvOpenhomeOrgPlaylist1.h>
#include <OpenHome/Av/Qobuz/QobuzMetadata.h>

namespace OpenHome {
    class Environment;
    class IUnixTimestamp;
namespace Configuration {
    class IConfigInitialiser;
    class ConfigChoice;
}
namespace Av {

class Qobuz : public ICredentialConsumer
{
    friend class TestQobuz;
    friend class QobuzPins;
    static const TUint kReadBufferBytes = 4 * 1024;
    static const TUint kWriteBufferBytes = 1024;
    static const TUint kConnectTimeoutMs = 10000; // FIXME - should read this + ProtocolNetwork's equivalent from a single client-changable location
    static const Brn kHost;
    static const TUint kPort = 80;
    static const TUint kGranularityUsername = 128;
    static const TUint kGranularityPassword = 128;
    static const Brn kId;
    static const Brn kVersionAndFormat;
    static const TUint kSecsBetweenNtpAndUnixEpoch = 2208988800; // secs between 1900 and 1970
    static const TUint kMaxStatusBytes = 512;
    static const TUint kMaxPathAndQueryBytes = 512;
public:
    static const Brn kConfigKeySoundQuality;
public:
    Qobuz(Environment& aEnv, const Brx& aAppId, const Brx& aAppSecret,
          ICredentialsState& aCredentialsState, Configuration::IConfigInitialiser& aConfigInitialiser,
          IUnixTimestamp& aUnixTimestamp);
    ~Qobuz();
    TBool TryLogin();
    TBool TryGetStreamUrl(const Brx& aTrackId, Bwx& aStreamUrl);
    TBool TryGetId(IWriter& aWriter, const Brx& aQuery, QobuzMetadata::EIdType aType);
    TBool TryGetIds(IWriter& aWriter, const Brx& aGenre, QobuzMetadata::EIdType aType, TUint aMaxAlbumsPerResponse);
    TBool TryGetIdsByRequest(IWriter& aWriter, const Brx& aRequestUrl, TUint aMaxAlbumsPerResponse);
    TBool TryGetGenreList(IWriter& aWriter);
    TBool TryGetTracksById(IWriter& aWriter, const Brx& aId, QobuzMetadata::EIdType aType, TUint aLimit, TUint aOffset);
    TBool TryGetTracksByRequest(IWriter& aWriter, const Brx& aRequestUrl, TUint aLimit, TUint aOffset);
    void Interrupt(TBool aInterrupt);
private: // from ICredentialConsumer
    const Brx& Id() const override;
    void CredentialsChanged(const Brx& aUsername, const Brx& aPassword) override;
    void UpdateStatus() override;
    void Login(Bwx& aToken) override;
    void ReLogin(const Brx& aCurrentToken, Bwx& aNewToken) override;
private:
    TBool TryConnect();
    TBool TryLoginLocked();
    TUint WriteRequestReadResponse(const Brx& aMethod, const Brx& aHost, const Brx& aPathAndQuery);
    TBool TryGetResponse(IWriter& aWriter, const Brx& aHost, TUint aLimit, TUint aOffset);
    Brn ReadString();
    void QualityChanged(Configuration::KeyValuePair<TUint>& aKvp);
    static void AppendMd5(Bwx& aBuffer, const Brx& aToHash);
private:
    Environment& iEnv;
    Mutex iLock;
    Mutex iLockConfig;
    ICredentialsState& iCredentialsState;
    IUnixTimestamp& iUnixTimestamp;
    SocketTcpClient iSocket;
    Srs<1024> iReaderBuf;
    ReaderUntilS<1024> iReaderUntil1;
    Sws<kWriteBufferBytes> iWriterBuf;
    WriterHttpRequest iWriterRequest;
    ReaderHttpResponse iReaderResponse;
    ReaderHttpChunked iDechunker;
    ReaderUntilS<kReadBufferBytes> iReaderUntil2;
    HttpHeaderContentLength iHeaderContentLength;
    HttpHeaderTransferEncoding iHeaderTransferEncoding;
    const Bws<32> iAppId;
    const Bws<32> iAppSecret;
    WriterBwh iUsername;
    WriterBwh iPassword;
    TUint iSoundQuality;
    Bws<128> iAuthToken;
    Bws<512> iPathAndQuery; // slightly too large for the stack; requires that all network operations are serialised
    Configuration::ConfigChoice* iConfigQuality;
    TUint iSubscriberIdQuality;
};

};  // namespace Av
};  // namespace OpenHome


