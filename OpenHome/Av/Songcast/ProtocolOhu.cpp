#include <OpenHome/Av/Songcast/ProtocolOhu.h>
#include <OpenHome/Media/Protocol/Protocol.h>
#include <OpenHome/Av/Songcast/Ohm.h>
#include <OpenHome/Av/Songcast/OhmMsg.h>
#include <OpenHome/Av/Songcast/OhmSocket.h>
#include <OpenHome/Av/Songcast/ProtocolOhBase.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/Private/Env.h>
#include <OpenHome/Functor.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Av/Debug.h>
#include <OpenHome/PowerManager.h>
#include <OpenHome/Media/ClockPuller.h>

using namespace OpenHome;
using namespace OpenHome::Av;
using namespace OpenHome::Media;

// ProtocolOhu

ProtocolOhu::ProtocolOhu(Environment& aEnv, IOhmMsgFactory& aMsgFactory, Media::TrackFactory& aTrackFactory, Optional<IOhmTimestamper> aTimestamper, const Brx& aMode, Optional<Av::IOhmMsgProcessor> aOhmMsgProcessor)
    : ProtocolOhBase(aEnv, aMsgFactory, aTrackFactory, aTimestamper, "ohu", aMode, aOhmMsgProcessor)
    , iLeaveLock("POHU")
{
    iTimerLeave = new Timer(aEnv, MakeFunctor(*this, &ProtocolOhu::TimerLeaveExpired), "ProtocolOhuLeave");
}

ProtocolOhu::~ProtocolOhu()
{
    delete iTimerLeave;
}

void ProtocolOhu::HandleAudio(const OhmHeader& aHeader)
{
    Broadcast(iMsgFactory.CreateAudio(iReadBuffer, aHeader));

    AutoMutex a(iLeaveLock);
    if (iLeaving) {
        iTimerLeave->Cancel();
        // Ensure a JOIN/LISTEN doesn't go out after a LEAVE.
        iTimerJoin->Cancel();
        iTimerListen->Cancel();
        SendLeave();
        iReadBuffer.ReadInterrupt();
    }
}

void ProtocolOhu::HandleTrack(const OhmHeader& aHeader)
{
    Broadcast(iMsgFactory.CreateTrack(iReadBuffer, aHeader));
}

void ProtocolOhu::HandleMetatext(const OhmHeader& aHeader)
{
    Broadcast(iMsgFactory.CreateMetatext(iReadBuffer, aHeader));
}

void ProtocolOhu::HandleSlave(const OhmHeader& aHeader)
{
    OhmHeaderSlave headerSlave;
    headerSlave.Internalise(iReadBuffer, aHeader);
    iSlaveCount = headerSlave.SlaveCount();

    for (TUint i = 0; i < iSlaveCount; i++) {
        iSlaveList[i].Internalise(iReadBuffer);
        if (Debug::TestLevel(Debug::kSongcast)) {
            Endpoint::EndpointBuf endptBuf;
            iSlaveList[i].AppendEndpoint(endptBuf);
            LOG(kSongcast, "ProtocolOhu - slave #%u - %s\n", i, endptBuf.Ptr());
        }
    }
}

void ProtocolOhu::Broadcast(OhmMsg* aMsg)
{
    if (iSlaveCount > 0) {
        WriterBuffer writer(iMessageBuffer);
        writer.Flush();
        aMsg->Externalise(writer);
        for (TUint i = 0; i < iSlaveCount; i++) {
            try {
                iSocket.Send(iMessageBuffer, iSlaveList[i]);
            }
            catch (NetworkError&) {
                Endpoint::EndpointBuf buf;
                iSlaveList[i].AppendEndpoint(buf);
                LOG_ERROR(kApplication6, "NetworkError in ProtocolOhu::Broadcast for slave %s\n", buf.Ptr());
            }
        }
    }

    Add(aMsg);
}

ProtocolStreamResult ProtocolOhu::Play(TIpAddress /*aInterface*/, TUint aTtl, const Endpoint& aEndpoint)
{
    LOG(kSongcast, "OHU: Play(%08x, %u, %08x:%u\n", iAddr, aTtl, aEndpoint.Address(), aEndpoint.Port());
    if (aEndpoint.Address() == 0) {
        // ohu null address, return immediately
        return EProtocolStreamStopped;
    }
    iLeaveLock.Wait();
    iLeaving = iStopped = false;
    iSlaveCount = 0;
    iNextFlushId = MsgFlush::kIdInvalid;
    iLeaveLock.Signal();
    iEndpoint.Replace(aEndpoint);
    TBool firstJoin = true;
    do {
        if (!firstJoin) {
            // Ensure a JOIN/LISTEN doesn't go out after a LEAVE.
            iTimerJoin->Cancel();
            iTimerListen->Cancel();
            SendLeave();
            /* allow lower priority threads to run.  If all network operations are failing
               (say because we have no IP address), this high priority thread will loop. */
            Thread::Sleep(50);
        }
        WaitForPipelineToEmpty();
        iLeaveLock.Wait();
        if (iStarving && !iStopped) {
            iStarving = false;
            iSocket.Interrupt(false);
        }

        try {
            AutoMutex _(iMutexTransport);
            iSocket.Close();
            iSocket.OpenUnicast(iAddr, aTtl);
        }
        catch (NetworkError&) {
            return EProtocolStreamErrorUnrecoverable;
        }

        if (iTimestamper != nullptr) {
            iTimestamper->Stop();
            iTimestamper->Start(iSocket.This());
        }

        iLeaveLock.Signal();
        try {
            OhmHeader header;
            SendJoin();

            // Phase 1, periodically send join until Track and Metatext have been received
            TBool joinComplete = false;
            TBool receivedTrack = false;
            TBool receivedMetatext = false;

            while (!joinComplete) {
                try {
                    header.Internalise(iReadBuffer);

                    switch (header.MsgType())
                    {
                    case OhmHeader::kMsgTypeJoin:
                    case OhmHeader::kMsgTypeListen:
                    case OhmHeader::kMsgTypeLeave:
                        break;
                    case OhmHeader::kMsgTypeAudio:
                    {
                        /* Ignore audio while joining - it might be from while we were waiting
                           for the pipeline to empty if we're re-starting a stream following a drop-out
                           We do however need to check for timestamps, to avoid the timestamper
                           filling up with out of date values */
                        auto lmsg = iMsgFactory.CreateAudio(iReadBuffer, header);
                        AddRxTimestamp(*lmsg);
                        lmsg->RemoveRef();
                    }
                        break;
                    case OhmHeader::kMsgTypeTrack:
                        LOG(kSongcast, "OHU: Joining, received track\n");
                        HandleTrack(header);
                        receivedTrack = true;
                        joinComplete = receivedMetatext;
                        break;
                    case OhmHeader::kMsgTypeMetatext:
                        LOG(kSongcast, "OHU: Joining, metatext\n");
                        HandleMetatext(header);
                        receivedMetatext = true;
                        joinComplete = receivedTrack;
                        break;
                    case OhmHeader::kMsgTypeSlave:
                        HandleSlave(header);
                        break;
                    case OhmHeader::kMsgTypeResend:
                        ResendSeen();
                        break;
                    default:
                        ASSERTS();
                    }

                    iReadBuffer.ReadFlush();
                }
                catch (OhmError&) {
                    LOG_ERROR(kSongcast, "OHU: OhmError while joining\n");
                }
            }
            
            iTimerJoin->Cancel();
            LOG(kSongcast, "OHU: Joined\n");
            if (firstJoin) {
                /* Put pipeline into Waiting state initially in case sender is currently paused.
                   Subsequent loops will likely be prompted by network starvation.  We don't want
                   to output a Wait in this case; its correct that the pipeline goes Buffering */
                iSupply->OutputWait();
                firstJoin = false;
            }

            // Phase 2, periodically send listen if required
            iTimerListen->FireIn((kTimerListenTimeoutMs >> 2) - iEnv.Random(kTimerListenTimeoutMs >> 3)); // listen primary timeout
            for (;;) {
                try {
                    header.Internalise(iReadBuffer);

                    switch (header.MsgType())
                    {
                    case OhmHeader::kMsgTypeJoin:
                    case OhmHeader::kMsgTypeLeave:
                        break;
                    case OhmHeader::kMsgTypeListen:
                        iTimerListen->FireIn((kTimerListenTimeoutMs >> 1) - iEnv.Random(kTimerListenTimeoutMs >> 3)); // listen secondary timeout
                        break;
                    case OhmHeader::kMsgTypeAudio:
                        HandleAudio(header);
                        break;
                    case OhmHeader::kMsgTypeTrack:
                        HandleTrack(header);
                        break;
                    case OhmHeader::kMsgTypeMetatext:
                        HandleMetatext(header);
                        break;
                    case OhmHeader::kMsgTypeSlave:
                        HandleSlave(header);
                        break;
                    case OhmHeader::kMsgTypeResend:
                        ResendSeen();
                        break;
                    default:
                        ASSERTS();
                    }

                    iReadBuffer.ReadFlush();
                }
                catch (OhmError&) {
                    LOG_ERROR(kSongcast, "OHU: OhmError while playing\n");
                }
            }
        }
        catch (NetworkError&) { // from OpenUnicast only
            LOG_ERROR(kSongcast, "OHU: NetworkError.  Stopped=%u, starving=%u, leaving=%u\n", iStopped, iStarving, iLeaving);
        }
        catch (ReaderError&) {
            LOG_ERROR(kSongcast, "OHU: ReaderError.  Stopped=%u, starving=%u, leaving=%u\n", iStopped, iStarving, iLeaving);
        }
        catch (OhmDiscontinuity&) {
            LOG_ERROR(kSongcast, "OHU: Sender Halted.  Stopped=%u, starving=%u, leaving=%u\n", iStopped, iStarving, iLeaving);
        }
    } while (!iStopped);

    if (iTimestamper != nullptr) {
        iTimestamper->Stop();
    }

    Interrupt(false); // cancel any interrupt to allow SendLeave to succeed
    iReadBuffer.ReadFlush();
    // Ensure a JOIN/LISTEN doesn't go out after SendLeave() is called, which may confuse a sender and think this receiver has immediately re-joined.
    iTimerJoin->Cancel();
    iTimerListen->Cancel();
    // About to directly call SendLeave() below, if outstanding, so cancel LEAVE timer here.
    iTimerLeave->Cancel();
    iLeaveLock.Wait();
    if (iLeaving) {
        iLeaving = false;
        SendLeave();
    }
    iLeaveLock.Signal();
    iSocket.Close();
    iMutexTransport.Wait();
    iStreamId = IPipelineIdProvider::kStreamIdInvalid;
    const TUint flushId = iNextFlushId;
    iNextFlushId = MsgFlush::kIdInvalid;
    iMutexTransport.Signal();
    if (flushId != MsgFlush::kIdInvalid) {
        iSupply->OutputFlush(flushId);
    }
    return iStopped? EProtocolStreamStopped : EProtocolStreamErrorUnrecoverable;
}

void ProtocolOhu::Interrupt(TBool aInterrupt)
{
    LOG(kSongcast, "OHU: Interrupt(%u)\n", aInterrupt);
    if (aInterrupt) {
        iLeaveLock.Wait();
        iStopped = aInterrupt;
        iLeaving = aInterrupt;
        iLeaveLock.Signal();
    }
    ProtocolOhBase::Interrupt(aInterrupt);
}

TUint ProtocolOhu::TryStop(TUint aStreamId)
{
    LOG(kSongcast, "OHU: TryStop(%u)\n", aStreamId);
    AutoMutex _(iMutexTransport);
    if (IsCurrentStream(aStreamId) && iStreamId == aStreamId) {
        AutoMutex a(iLeaveLock);
        if (iNextFlushId == MsgFlush::kIdInvalid) {
            iNextFlushId = iFlushIdProvider->NextFlushId();
        }
        iStopped = true;
        iLeaving = true;
        iTimerLeave->FireIn(kTimerLeaveTimeoutMs);
        iReadBuffer.ReadInterrupt();
    }
    return iNextFlushId;
}

void ProtocolOhu::SendLeave()
{
    Send(OhmHeader::kMsgTypeLeave);
}

void ProtocolOhu::TimerLeaveExpired()
{
    // Ensure a JOIN/LISTEN doesn't go out after a LEAVE.
    iTimerJoin->Cancel();
    iTimerListen->Cancel();
    AutoMutex a (iLeaveLock);
    if (!iLeaving) {
        return;
    }
    iLeaving = false;
    SendLeave();
    iReadBuffer.ReadInterrupt();
}
