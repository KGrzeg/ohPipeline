#include <OpenHome/Media/Protocol/Protocol.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Media/Debug.h>
#include <OpenHome/Av/Radio/ContentProcessorFactory.h>

/**
 *
 * https://tools.ietf.org/html/draft-pantos-http-live-streaming-14
 *
 * HLS uses a variant of M3U which adds its own tags of the form:
 *  #EXT-X-*
 *
 * In particular, a "master playlist" M3U/M3U8 (utf-8 encoded M3U) should
 * contain at least one stream variant (which will be a link to another M3U containing
 * one or more renditions).
 *
 * The stream variant tag has the following form:
 *  #EXT-X-STREAM-INF
 *
 *
 * An example master playlist is as follows:

   #EXTM3U
   #EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000
   http://example.com/low.m3u8
   #EXT-X-STREAM-INF:BANDWIDTH=2560000,AVERAGE-BANDWIDTH=2000000
   http://example.com/mid.m3u8
   #EXT-X-STREAM-INF:BANDWIDTH=7680000,AVERAGE-BANDWIDTH=6000000
   http://example.com/hi.m3u8
   #EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS="mp4a.40.5"
   http://example.com/audio-only.m3u8

 */


namespace OpenHome {
namespace Av {

class ContentM3uX : public Media::ContentProcessor
{
    static const TUint kMaxLineBytes = 2 * 1024;
    static const Brn kSchemeHttp;
    static const Brn kSchemeHttps;
    static const Brn kSchemeHls;
    static const Brn kSchemeHlsSecure;
public:
    ContentM3uX();
    ~ContentM3uX();
private: // from ContentProcessor
    TBool Recognise(const Brx& aUri, const Brx& aMimeType, const Brx& aData) override;
    void Reset() override;
    Media::ProtocolStreamResult Stream(IReader& aReader, TUint64 aTotalBytes) override;
private:
    void CacheUri(const Brx& aResource);
    void StoreHlsUriAbsolute(const Uri& aUri);
    void StoreHlsUriRelative(const Uri& aUri, const Brx& aResource);
private:
    static const Brn StripUriResource(const Uri& aUri);
    static const Brx& ConvertScheme(const Brx& aScheme);
private:
    ReaderUntil* iReaderUntil;
    Uri iUriPlaylist;
    Uri iUriHls;
    TUint iBandwidth;
    TBool iIsAudio;
    TBool iCacheNextUri;
};

} // namespace Av
} // namespace OpenHome

using namespace OpenHome;
using namespace OpenHome::Media;
using namespace OpenHome::Av;


ContentProcessor* ContentProcessorFactory::NewM3uX()
{ // static
    return new ContentM3uX();
}


// ContentM3uX

const Brn ContentM3uX::kSchemeHttp("http");
const Brn ContentM3uX::kSchemeHttps("https");
const Brn ContentM3uX::kSchemeHls("hls");
const Brn ContentM3uX::kSchemeHlsSecure("hlss");

ContentM3uX::ContentM3uX()
{
    iReaderUntil = new ReaderUntilS<kMaxLineBytes>(*this);
}

ContentM3uX::~ContentM3uX()
{
    delete iReaderUntil;
}

TBool ContentM3uX::Recognise(const Brx& aUri, const Brx& aMimeType, const Brx& aData)
{
    try {
        iUriPlaylist.Replace(aUri);
    }
    catch (UriError&) {
        return false;
    }

    if (Ascii::CaseInsensitiveEquals(aMimeType, Brn("application/x-mpegurl")) ||
        Ascii::CaseInsensitiveEquals(aMimeType, Brn("application/vnd.apple.mpegurl"))/* ||
        Ascii::CaseInsensitiveEquals(aMimeType, Brn("audio/x-mpegurl")) ||
        Ascii::CaseInsensitiveEquals(aMimeType, Brn("audio/mpegurl"))*/) {
        // Comparing against audio/x-mpegurl or audio/mpegurl alone could
        // clash with other M3U files.
        return true;
    }
    if (Ascii::Contains(aData, Brn("#EXTM3U")) &&
        Ascii::Contains(aData, Brn("#EXT-X-STREAM-INF"))) {
        return true;
    }
    return false;
}

void ContentM3uX::Reset()
{
    iReaderUntil->ReadFlush();
    ContentProcessor::Reset();
    iUriPlaylist.Clear();
    iUriHls.Clear();
    iBandwidth = 0;
    iIsAudio = false;
    iCacheNextUri = false;
}

ProtocolStreamResult ContentM3uX::Stream(IReader& aReader, TUint64 aTotalBytes)
{
    LOG(kMedia, "ContentM3uX::Stream\n");

    SetStream(aReader);
    TUint64 bytesRemaining = aTotalBytes;
    try {
        for (;;) {
            Brn line = ReadLine(*iReaderUntil, bytesRemaining);
            if (line.Bytes() == 0) {
                continue; // empty/comment line
            }

            // Only want to stream one variant, but one or more may fail.
            // If that is the case, definitely want to fall through to other variants.
            if (line.BeginsWith(Brn("#EXT-X-STREAM-INF"))) {
                TUint attribBandwidth = 0;
                TBool attribAudio = false;
                Parser p(line);
                (void)p.Next(':');  // Discard "#EXT-X-STREAM-INF:".
                while (!p.Finished()) {
                    Brn attrib = p.Next(',');
                    Parser attribParser(attrib);
                    Brn attribName = attribParser.Next('=');
                    Brn attribValue = attribParser.Next();
                    if (attribName == Brn("BANDWIDTH")) {   // required attrib
                        attribBandwidth = Ascii::Uint(attribValue);
                    }
                    else if (attribName == Brn("CODECS")) {
                        static const Brn kAudioCodecPrefix("\"mp4a");
                        if (attribValue.Bytes() >= kAudioCodecPrefix.Bytes()) {
                            if (Brn(attribValue.Ptr(), kAudioCodecPrefix.Bytes()) == kAudioCodecPrefix) {
                                attribAudio = true;
                            }
                        }
                    }
                }

                if (attribAudio && !iIsAudio) {
                    // Haven't found an audio-only stream yet, so cache it.
                    iCacheNextUri = true;
                    iBandwidth = attribBandwidth;
                    iIsAudio = true;
                }
                else if (attribAudio && attribBandwidth > iBandwidth) {
                    // Found higher-bandwidth audio-only stream, so cache it.
                    iCacheNextUri = true;
                    iBandwidth = attribBandwidth;
                }
                else if (!iIsAudio && attribBandwidth > iBandwidth) {
                    // Not explicitly an audio-only stream, but higher-bandwidth than existing, so cache it.
                    iCacheNextUri = true;
                    iBandwidth = attribBandwidth;
                }
            }
            else if (iCacheNextUri) {
                CacheUri(line);
            }
        }
    }
    catch (ReaderError&) {
    }

    // Should get here via a ReaderError.
    // Need to check if entire M3U has been read, or if there was an unexpected break in stream.
    if (iUriHls.AbsoluteUri().Bytes() == 0 && bytesRemaining == 0 && bytesRemaining < aTotalBytes) {
        // Parsed entire file and didn't find stream.
        return EProtocolStreamErrorUnrecoverable;
    }
    else if (iUriHls.AbsoluteUri().Bytes() == 0 && bytesRemaining > 0 && bytesRemaining < aTotalBytes) {
        // Started parsing, but didn't finish.
        return EProtocolStreamErrorRecoverable;
    }
    else if (iUriHls.AbsoluteUri().Bytes() == 0) {
        // Didn't find URL for unknown reason; give up.
        return EProtocolStreamErrorUnrecoverable;
    }

    TBool streamSucceeded = false;
    ProtocolStreamResult res = iProtocolSet->Stream(iUriHls.AbsoluteUri());
    if (res == EProtocolStreamStopped) {
        return EProtocolStreamStopped;
    }
    else if (res == EProtocolStreamSuccess) {
        streamSucceeded = true;
    }

    if (!streamSucceeded && bytesRemaining > 0 && bytesRemaining < aTotalBytes) {
        // break in stream.  Return an error and let caller attempt to re-establish connection
        return EProtocolStreamErrorRecoverable;
    }
    else if (streamSucceeded) {
        return EProtocolStreamSuccess;
    }
    return EProtocolStreamErrorUnrecoverable;
}

void ContentM3uX::CacheUri(const Brx& aResource)
{
    // Check to see if URL is absolute.
    Uri absoluteUri;
    try {
        absoluteUri.Replace(aResource);
    }
    catch (UriError&) {
        // Not an absolute URL.
    }

    try {
        if (absoluteUri.AbsoluteUri().Bytes() > 0) {
            StoreHlsUriAbsolute(absoluteUri); // May throw UriError.
        }
        else {
            StoreHlsUriRelative(iUriPlaylist, aResource); // May throw UriError.
        }
    }
    catch (UriError&) {
        iUriHls.Clear();
    }
    iCacheNextUri = false;
}

void ContentM3uX::StoreHlsUriAbsolute(const Uri& aUri)
{
    const auto& scheme = aUri.Scheme();
    Bws<Uri::kMaxUriBytes> uri(ConvertScheme(scheme)); // ConvertScheme() may throw UriError.
    const TUint offset = scheme.Bytes();
    uri.Append(aUri.AbsoluteUri().Ptr()+offset, aUri.AbsoluteUri().Bytes()-offset);
    iUriHls.Replace(uri);
}

void ContentM3uX::StoreHlsUriRelative(const Uri& aUri, const Brx& aResource)
{
    // Uri::Replace(aBaseUri, aRelativeUri) expects aBaseUri to have been stripped of the resource it points to, so do that here.
    const auto uriStrippedBuf = StripUriResource(aUri);
    const Uri uriStripped(uriStrippedBuf, aResource); // May throw UriError.

    StoreHlsUriAbsolute(uriStripped);
}

const Brn ContentM3uX::StripUriResource(const Uri& aUri)
{
    TUint idx = 0;
    Parser p(aUri.AbsoluteUri());
    while (!p.Finished()) {
        const Brn next = p.Next('/');
        if (!p.Finished()) {
            idx = p.Index();
        }
    }
    const Brn uri(aUri.AbsoluteUri().Ptr(), idx);
    return uri;
}

const Brx& ContentM3uX::ConvertScheme(const Brx& aScheme)
{
    if (Ascii::CaseInsensitiveEquals(aScheme, kSchemeHttp)) {
        return kSchemeHls;
    }
    else if (Ascii::CaseInsensitiveEquals(aScheme, kSchemeHttps)) {
        return kSchemeHlsSecure;
    }
    else {
        LOG(kMedia, "ContentM3uX::ConvertScheme Don't know how to handle aScheme: %.*s\n", PBUF(aScheme));
        THROW(UriError);
    }
}
