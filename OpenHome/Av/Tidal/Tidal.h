#pragma once

#include <OpenHome/Av/Credentials.h>
#include <OpenHome/Types.h>
#include <OpenHome/SocketSsl.h>
#include <OpenHome/Configuration/ConfigManager.h>
#include <OpenHome/Private/Http.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Net/Private/DviStack.h>
#include <OpenHome/Av/MediaPlayer.h>
#include <Generated/CpAvOpenhomeOrgPlaylist1.h>
#include <OpenHome/Av/Tidal/TidalMetadata.h>
        
namespace OpenHome {
    class Environment;
namespace Configuration {
    class IConfigInitialiser;
    class ConfigChoice;
}
namespace Av {

class Tidal : public ICredentialConsumer
{
    friend class TestTidal;
    static const TUint kReadBufferBytes = 4 * 1024;
    static const TUint kWriteBufferBytes = 1024;
    static const TUint kConnectTimeoutMs = 10000; // FIXME - should read this + ProtocolNetwork's equivalent from a single client-changable location
    static const Brn kHost;
    static const TUint kPort = 443;
    static const TUint kGranularityUsername = 128;
    static const TUint kGranularityPassword = 128;
    static const Brn kId;
    static const TUint kMaxStatusBytes = 512;
    static const TUint kMaxPathAndQueryBytes = 512;
public:
    static const Brn kConfigKeySoundQuality;
public:
    Tidal(Environment& aEnv, const Brx& aToken, ICredentialsState& aCredentialsState, Configuration::IConfigInitialiser& aConfigInitialiser);
    ~Tidal();
    TBool TryLogin(Bwx& aSessionId);
    TBool TryReLogin(const Brx& aCurrentToken, Bwx& aNewToken);
    TBool TryGetStreamUrl(const Brx& aTrackId, Bwx& aStreamUrl);
    TBool TryLogout(const Brx& aSessionId);
    TBool TryGetId(WriterBwh& aWriter, const Brx& aQuery, TidalMetadata::EIdType aType);
    TBool TryGetIds(WriterBwh& aWriter, const Brx& aMood, TidalMetadata::EIdType aType, TUint aMaxAlbumsPerResponse);
    TBool TryGetTracksById(WriterBwh& aWriter, const Brx& aId, TidalMetadata::EIdType aType, TUint aLimit, TUint aOffset);
    void Interrupt(TBool aInterrupt);
private: // from ICredentialConsumer
    const Brx& Id() const override;
    void CredentialsChanged(const Brx& aUsername, const Brx& aPassword) override;
    void UpdateStatus() override;
    void Login(Bwx& aToken) override;
    void ReLogin(const Brx& aCurrentToken, Bwx& aNewToken) override;
private:
    TBool TryConnect(TUint aPort);
    TBool TryLoginLocked();
    TBool TryLoginLocked(Bwx& aSessionId);
    TBool TryLogoutLocked(const Brx& aSessionId);
    TBool TryGetSubscriptionLocked();
    TBool TryGetResponse(WriterBwh& aWriter, Bwx& aPathAndQuery, TUint aLimit, TUint aOffset);
    void WriteRequestHeaders(const Brx& aMethod, const Brx& aPathAndQuery, TUint aPort, TUint aContentLength = 0);
    static Brn ReadInt(ReaderUntil& aReader, const Brx& aTag);
    static Brn ReadString(ReaderUntil& aReader, const Brx& aTag);
    void QualityChanged(Configuration::KeyValuePair<TUint>& aKvp);
private:
    Mutex iLock;
    Mutex iLockConfig;
    ICredentialsState& iCredentialsState;
    SocketSsl iSocket;
    Srs<1024> iReaderBuf;
    ReaderUntilS<kReadBufferBytes> iReaderUntil;
    Sws<kWriteBufferBytes> iWriterBuf;
    WriterHttpRequest iWriterRequest;
    ReaderHttpResponse iReaderResponse;
    HttpHeaderContentLength iHeaderContentLength;
    const Bws<32> iToken;
    WriterBwh iUsername;
    WriterBwh iPassword;
    TUint iSoundQuality;
    TUint iMaxSoundQuality;
    Bws<16> iUserId;
    Bws<64> iSessionId;
    Bws<8> iCountryCode;
    Bws<1024> iStreamUrl;
    Configuration::ConfigChoice* iConfigQuality;
    TUint iSubscriberIdQuality;
};

};  // namespace Av
};  // namespace OpenHome


