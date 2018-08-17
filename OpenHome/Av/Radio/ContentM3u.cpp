#include <OpenHome/Media/Protocol/Protocol.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Media/Debug.h>
#include <OpenHome/Av/Radio/ContentProcessorFactory.h>
#include <OpenHome/Media/MimeTypeList.h>

/* Example pls file

#EXTM3U

#EXTINF:123,Sample title
C:\Documents and Settings\I\My Music\Sample.mp3

#EXTINF:321,Example title
C:\Documents and Settings\I\My Music\Greatest Hits\Example.ogg

*/

namespace OpenHome {
namespace Av {

class ContentM3u : public Media::ContentProcessor
{
    static const TUint kMaxLineBytes = 2 * 1024;
    static const Brn kExtension;
public:
    ContentM3u(Media::IMimeTypeList& aMimeTypeList);
    ~ContentM3u();
private: // from ContentProcessor
    TBool Recognise(const Brx& aUri, const Brx& aMimeType, const Brx& aData) override;
    Media::ProtocolStreamResult Stream(IReader& aReader, TUint64 aTotalBytes) override;
    void Reset() override;
private:
    ReaderUntil* iReaderUntil;
};

} // namespace Av
} // namespace OpenHome

using namespace OpenHome;
using namespace OpenHome::Media;
using namespace OpenHome::Av;


ContentProcessor* ContentProcessorFactory::NewM3u(IMimeTypeList& aMimeTypeList)
{ // static
    return new ContentM3u(aMimeTypeList);
}


// ContentM3u

const TUint ContentM3u::kMaxLineBytes;
const Brn ContentM3u::kExtension(".m3u");

ContentM3u::ContentM3u(IMimeTypeList& aMimeTypeList)
{
    iReaderUntil = new ReaderUntilS<kMaxLineBytes>(*this);
    aMimeTypeList.Add("audio/x-mpegurl");
    aMimeTypeList.Add("audio/mpegurl");
}

ContentM3u::~ContentM3u()
{
    delete iReaderUntil;
}

TBool ContentM3u::Recognise(const Brx& aUri, const Brx& aMimeType, const Brx& aData)
{
    if (Ascii::CaseInsensitiveEquals(aMimeType, Brn("audio/x-mpegurl")) ||
        Ascii::CaseInsensitiveEquals(aMimeType, Brn("audio/mpegurl"))) {
        return true;
    }
    if (Ascii::Contains(aData, Brn("#EXTM3U")) && !Ascii::Contains(aData, Brn("#EXT-X-"))) {
        return true;
    }

    /*
     * Fall back to checking file extension.
     * M3U files do not need to contain any kind of "header" or "recognition"
     * data (they may contain just a URI) so are not self-contained. If the
     * above checks fail, the only way of recognising an M3U is to check the
     * file extension (assuming the file extension is correct!).
     */
    Uri uri(aUri);
    const auto& path = uri.Path();
    // File extension must be at end of path portion of URI.
    if (path.Bytes() >= kExtension.Bytes()) {
        Brn extension(path.Ptr() + (path.Bytes() - kExtension.Bytes()), kExtension.Bytes());
        if (Ascii::CaseInsensitiveEquals(extension, kExtension)) {
            return true;
        }
    }

    return false;
}

ProtocolStreamResult ContentM3u::Stream(IReader& aReader, TUint64 aTotalBytes)
{
    LOG(kMedia, "ContentM3u::Stream\n");

    SetStream(aReader);
    TUint64 bytesRemaining = aTotalBytes;
    TBool stopped = false;
    TBool streamSucceeded = false;
    try {
        while (!stopped) {
            Brn line = ReadLine(*iReaderUntil, bytesRemaining);
            if (line.Bytes() == 0 || line.BeginsWith(Brn("#"))) {
                continue; // empty/comment line
            }
            ProtocolStreamResult res = iProtocolSet->Stream(line);
            if (res == EProtocolStreamStopped) {
                stopped = true;
            }
            else if (res == EProtocolStreamSuccess) {
                streamSucceeded = true;
            }
        }
    }
    catch (ReaderError&) {
    }

    if (stopped) {
        return EProtocolStreamStopped;
    }
    else if (bytesRemaining > 0 && bytesRemaining < aTotalBytes) {
        // break in stream.  Return an error and let caller attempt to re-establish connection
        return EProtocolStreamErrorRecoverable;
    }
    else if (streamSucceeded) {
        return EProtocolStreamSuccess;
    }
    return EProtocolStreamErrorUnrecoverable;
}

void ContentM3u::Reset()
{
    iReaderUntil->ReadFlush();
    ContentProcessor::Reset();
}
