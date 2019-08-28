#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Types.h>
#include <OpenHome/SocketSsl.h>
#include <OpenHome/Private/Http.h>

#include <algorithm>

using namespace OpenHome;
using namespace OpenHome::TestFramework;

static void TestHttps(Environment& aEnv, const Brx& aHost, const Brx& aPath)
{
    // FIXME - Debug::SetLevel(kSsl)
    static const TUint kWriteBufBytes = 2 * 1024;
    static const TUint kReadBufBytes = 4 * 1024;
    SslContext* ssl = new SslContext();
    SocketSsl* socket = new SocketSsl(aEnv, *ssl, kReadBufBytes);
    Srx* readBuffer = new Srs<1024>(*socket);
    ReaderUntil* readerUntil = new ReaderUntilS<kReadBufBytes>(*readBuffer);
    ReaderHttpResponse* readerResponse = new ReaderHttpResponse(aEnv, *readerUntil);
    Sws<kWriteBufBytes>* writeBuffer = new Sws<kWriteBufBytes>(*socket);
    WriterHttpRequest* writerRequest = new WriterHttpRequest(*writeBuffer);

    static const TUint kTimeoutMs = 5 * 1000;
    static const TUint kPort = 443;
    Endpoint ep(kPort, aHost);
    socket->Connect(ep, kTimeoutMs);
    //socket->LogVerbose(true);
    writerRequest->WriteMethod(Http::kMethodGet, aPath, Http::eHttp11);
    Http::WriteHeaderHostAndPort(*writerRequest, aHost, kPort);
    Http::WriteHeaderConnectionClose(*writerRequest);
    writerRequest->WriteFlush();

    HttpHeaderContentLength headerContentLength;
    HttpHeaderTransferEncoding headerTransferEncoding;
    readerResponse->AddHeader(headerContentLength);
    readerResponse->AddHeader(headerTransferEncoding);
    readerResponse->Read(kTimeoutMs);
    const HttpStatus& status = readerResponse->Status();
    if (status != HttpStatus::kOk) {
        Print("ERROR: %d, ", status.Code());
        Print(status.Reason());
        Print("\n");
    }
    else if (headerTransferEncoding.IsChunked()) {
        ReaderHttpChunked dechunker(*readerUntil);
        dechunker.SetChunked(true);
        Brn buf;
        for (;;) {
            buf.Set(dechunker.Read(1024));
            if (buf.Bytes() == 0) {
                break;
            }
            Print(buf);
        }
    }
    else {
        TUint length = headerContentLength.ContentLength();
        while (length > 0) {
            const TUint bytes = std::min(length, (TUint)1024);
            Brn buf = readerUntil->Read(bytes);
            Print(buf);
            length -= buf.Bytes();
        }
    }

    socket->Close();
    delete writerRequest;
    delete writeBuffer;
    delete readerResponse;
    delete readerUntil;
    delete readBuffer;
    delete socket;
    delete ssl;
}

void OpenHome::TestFramework::Runner::Main(TInt aArgc, TChar* aArgv[], Net::InitialisationParams* aInitParams)
{
    Environment* env = Net::UpnpLibrary::Initialise(aInitParams);
    OptionParser parser;
    OptionString optionHost("-h", "--host", Brn("www.ssllabs.com"), "host to connect to");
    parser.AddOption(&optionHost);
    OptionString optionPath("-p", "--path", Brn("/ssltest/viewMyClient.html"), "path on the host to (HTTP) GET");
    parser.AddOption(&optionPath);
    std::vector<Brn> args = OptionParser::ConvertArgs(aArgc, aArgv);
    if (!parser.Parse(args) || parser.HelpDisplayed()) {
        return;
    }
    TestHttps(*env, optionHost.Value(), optionPath.Value());
    Net::UpnpLibrary::Close();
}
