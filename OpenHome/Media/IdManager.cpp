#include <OpenHome/Media/IdManager.h>
#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Media/Debug.h>

#include <climits>

using namespace OpenHome;
using namespace OpenHome::Media;

//#define LogVerbose(prefix)
#define LogVerbose(prefix) Log(prefix)

// IdManager

IdManager::IdManager(IStopper& aStopper)
    : iLock("IDPR")
    , iStopper(aStopper)
    , iNextStreamId(kStreamIdInvalid+1)
    , iIndexHead(0)
    , iIndexTail(0)
{
}

void IdManager::AddStream(TUint aId, TUint aStreamId, TBool aPlayNow)
{
    iLock.Wait();
    iActiveStreams[iIndexTail].Set(aId, aStreamId, aPlayNow);
    UpdateIndex(iIndexTail);
    ASSERT(iIndexHead != iIndexTail); // OkToPlay can't tell the difference between a full and empty list
                                      // ...so we assume the list contains at most kMaxActiveStreams-1 elements
    LOG(kPipeline, "IdManager::AddStream(%u, %u, %u)\n", aId, aStreamId, aPlayNow)
    iLock.Signal();
}

TUint IdManager::MaxStreams() const
{
    return kMaxActiveStreams;
}

inline void IdManager::UpdateIndex(TUint& aIndex)
{ // static
    if (++aIndex == kMaxActiveStreams) {
        aIndex = 0;
    }
}

TUint IdManager::UpdateId(TUint& aId)
{
    iLock.Wait();
    TUint id = aId++;
    iLock.Signal();
    return id;
}

void IdManager::Log(const TChar* aPrefix)
{
    Log::Print("IdManager: %s.  Pending items are:\n", aPrefix);
    TUint index = iIndexHead;
    while (index != iIndexTail) {
        ActiveStream& as = iActiveStreams[index];
        Log::Print("    trackId:%u streamId:%u, playNow=%u\n", as.Id(), as.StreamId(), as.PlayNow());
        if (++index == kMaxActiveStreams) {
            index = 0;
        }
    }
}

TUint IdManager::NextStreamId()
{
    return UpdateId(iNextStreamId);
}

EStreamPlay IdManager::OkToPlay(TUint aStreamId)
{
    AutoMutex a(iLock);
    if (iIndexHead == iIndexTail) {
        LOG(kPipeline, "IdManager::OkToPlay(%u) returning %s - no streams pending\n", aStreamId, kStreamPlayNames[ePlayNo]);
        return ePlayNo;
    }
    const ActiveStream& as = iActiveStreams[iIndexHead];
    if (as.StreamId() != aStreamId) {
        if (Debug::TestLevel(Debug::kPipeline)) {
            Log::Print("OkToPlay(%u) returning %s - wrong stream\n", aStreamId, kStreamPlayNames[ePlayNo]);
            Log("OkToPlay");
        }
        return ePlayNo;
    }
    iPlaying.Set(as);
    UpdateIndex(iIndexHead);
    EStreamPlay canPlay = (iPlaying.PlayNow()? ePlayYes : ePlayLater);
    LOG(kPipeline, "IdManager::OkToPlay(%u) returning %s\n", aStreamId, kStreamPlayNames[canPlay]);
    return canPlay;
}

void IdManager::InvalidateAt(TUint aId)
{
    AutoMutex a(iLock);

    TBool matched = false;
    if (iPlaying.Id() == aId) {
        matched = true;
        iStopper.RemoveStream(iPlaying.StreamId());
        iPlaying.Clear();
    }
    TBool updateHead = matched;

    if (iIndexHead == iIndexTail) {
        return;
    }
    TUint index = iIndexHead;
    TUint prevIndex = index;
    // find first match
    while (!matched && index != iIndexTail) {
        matched = (iActiveStreams[index].Id() == aId);
        if (matched && index == iIndexHead) {
            updateHead = true;
        }
        prevIndex = index;
        UpdateIndex(index);
    }

    if (matched) {
        // advance past any additional streams for the same track
        while (index != iIndexTail && iActiveStreams[index].Id() == aId) {
            UpdateIndex(index);
        }

        if (updateHead) {
            iIndexHead = index;
        }
        else { // shuffle remainder of buffer down
            ASSERT(prevIndex != index);
            for (;;) {
                if (index == iIndexTail) {
                    break;
                }
                iActiveStreams[prevIndex].Set(iActiveStreams[index]);
                UpdateIndex(prevIndex);
                UpdateIndex(index);
            }
            iIndexTail = prevIndex;
        }
    }
    if (Debug::TestLevel(Debug::kMedia)) {
        Bws<64> buf("InvalidateAt(");
        buf.AppendPrintf("%u", aId);
        buf.PtrZ();
        LogVerbose((const TChar*)buf.Ptr());
    }
}

void IdManager::InvalidateAfter(TUint aId)
{
    AutoMutex a(iLock);

    // find first matching instance
    TUint index = iIndexHead;
    TUint streamId = kStreamIdInvalid;
    TBool matched = (iPlaying.Id() == aId);
    if (matched) {
        streamId = iPlaying.StreamId();
    }
    while (!matched && index != iIndexTail) {
        if (iActiveStreams[index].Id() == aId) {
            matched = true;
            streamId = iActiveStreams[index].StreamId();
        }
        UpdateIndex(index);
    }

    // if matched, advance past any additional streams for the same track
    if (matched) {
        while (   index != iIndexTail
               && iActiveStreams[index].Id() == aId
               && streamId < iActiveStreams[index].StreamId()
               && iActiveStreams[index].PlayNow()) {
            streamId = iActiveStreams[index].StreamId();
            UpdateIndex(index);
        }
        iIndexTail = index;
    }
    if (Debug::TestLevel(Debug::kMedia)) {
        Bws<64> buf("InvalidateAfter(");
        buf.AppendPrintf("%u", aId);
        buf.PtrZ();
        LogVerbose((const TChar*)buf.Ptr());
    }
}

void IdManager::InvalidatePending()
{
    iLock.Wait();
    iIndexTail = iIndexHead;
    LOG(kMedia, "IdManager::InvalidatePending()\n");
    iLock.Signal();
}

void IdManager::InvalidateAll()
{
    AutoMutex a(iLock);
    if (!iPlaying.IsClear()) {
        iStopper.RemoveStream(iPlaying.StreamId());
        iPlaying.Clear();
    }
    iIndexTail = iIndexHead;
    LOG(kMedia, "IdManager::InvalidateAll()\n");
}


//  IdManager::ActiveStream

IdManager::ActiveStream::ActiveStream()
{
    Clear();
}

void IdManager::ActiveStream::Set(TUint aId, TUint aStreamId, TBool aPlayNow)
{
    iId = aId;
    iStreamId = aStreamId;
    iPlayNow = aPlayNow;
    iClear = false;
}

void IdManager::ActiveStream::Set(const ActiveStream& aActiveStream)
{
    Set(aActiveStream.Id(), aActiveStream.StreamId(), aActiveStream.PlayNow());
}

void IdManager::ActiveStream::Clear()
{
    iId = UINT_MAX;
    iStreamId = UINT_MAX;
    iPlayNow = false;
    iClear = true;
}
