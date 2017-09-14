#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Private/SuiteUnitTest.h>
#include <OpenHome/Av/Raop/Raop.h>
#include <OpenHome/Av/Raop/ProtocolRaop.h>
#include <OpenHome/Tests/TestPipe.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/Private/Ascii.h>

namespace OpenHome {
namespace Av {
namespace Test {

class MockResendRequester : public IResendRangeRequester, private INonCopyable
{
public:
    MockResendRequester(OpenHome::Test::ITestPipeWritable& aTestPipe);
private: // from IResendRangeRequester
    void RequestResendSequences(const std::vector<const IResendRange*> aRanges) override;
private:
    OpenHome::Test::ITestPipeWritable& iTestPipe;
};

class MockAudioSupply : public IAudioSupply, private INonCopyable
{
public:
    MockAudioSupply(OpenHome::Test::ITestPipeWritable& aTestPipe);
private: // from IAudioSupply
    void OutputAudio(const Brx& aAudio) override;
private:
    OpenHome::Test::ITestPipeWritable& iTestPipe;
};

class MockRepairableAllocator;

class MockRepairable : public IRepairable
{
public:
    MockRepairable(OpenHome::Test::ITestPipeWritable& aTestPipe, MockRepairableAllocator& aAllocator, TUint aMaxBytes);
    void Set(TUint aFrame, TBool aResend, const Brx& aData);
public: // from IRepairable
    TUint Frame() const override;
    TBool Resend() const override;
    const Brx& Data() const override;
    void Destroy() override;
private:
    OpenHome::Test::ITestPipeWritable& iTestPipe;
    MockRepairableAllocator& iAllocator;
    TUint iFrame;
    TBool iResend;
    Bwh iData;
};

class MockRepairableAllocator
{
public:
    MockRepairableAllocator(OpenHome::Test::ITestPipeWritable& aTestPipe, TUint aMaxRepairable, TUint aMaxBytes);
    ~MockRepairableAllocator();
    IRepairable* Allocate(TUint aFrame, TBool aResend, const Brx& aData);
    void Deallocate(MockRepairable* aRepairable);
private:
    FifoLiteDynamic<MockRepairable*> iFifo;
};

/*
 * Mock timer which does NOT report time passed in FireIn() calls to test pipe.
 *
 * Repairer randomises time passed to some FireIn() calls, so simplest solution
 * is to have timers report FireIn() calls without the unpredictable time
 * parameter.
 */
class MockTimerRepairer : public OpenHome::ITimer, private OpenHome::INonCopyable
{
public:
    MockTimerRepairer(OpenHome::Test::ITestPipeWritable& aTestPipe, OpenHome::Functor aCallback, const TChar* aId);
    const TChar* Id() const;
    void Fire();
public: // from ITimer
    void FireIn(TUint aMs) override;
    void Cancel() override;
private:
    OpenHome::Test::ITestPipeWritable& iTestPipe;
    OpenHome::Functor iCallback;
    const TChar* iId;
};

class MockTimerFactoryRepairer : public OpenHome::ITimerFactory, private OpenHome::INonCopyable
{
public:
    MockTimerFactoryRepairer(OpenHome::Test::ITestPipeWritable& aTestPipe);
    void FireTimer(const TChar* aId);
public: // from ITimerFactory
    ITimer* CreateTimer(OpenHome::Functor aCallback, const TChar* aId) override;
private:
    OpenHome::Test::ITestPipeWritable& iTestPipe;
    std::vector<std::reference_wrapper<MockTimerRepairer>> iTimers;
};

class SuiteRaopResend : public TestFramework::SuiteUnitTest, private INonCopyable
{
private:
    static const TUint kMaxFrames = 5;
    static const TUint kMaxFrameBytes = 5;  // Only expect to store string vals 0..65535.
    static const TUint kMaxTestPipeMessages = 50;
public:
    SuiteRaopResend(Environment& aEnv);
private: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestNoDropouts();
    void TestResendOnePacket();
    void TestResendMultiplePackets();
    void TestResendMulitpleRanges();
    void TestResendBeyondMultipleRangeLimit();
    void TestMultipleResendRecover();
    void TestResendRequest();
    void TestResendPacketBufferOverflowFirst();
    void TestResendPacketBufferOverflowMiddle();
    void TestResendPacketBufferOverflowLast();
    void TestResendBufferOverflowRecover();
    void TestResendPacketsOutOfOrder();
    void TestDropPacketWhileAwaitingResend();
    void TestResendPacketsAlreadySeen();
    void TestStreamReset();   // Receiving a packet already seen but is not a resend.
    void TestStreamResetResendPending();
    void TestDropAudio();
    void TestSequenceNumberWrapping();
    void TestSequenceNumberWrappingDuringRepair();
private:
    Environment& iEnv;
    OpenHome::Test::TestPipeDynamic* iTestPipe;
    MockResendRequester* iResendRequester;
    MockAudioSupply* iAudioSupply;
    MockTimerFactoryRepairer* iTimerFactory;
    MockRepairableAllocator* iAllocator;
    Repairer<kMaxFrames>* iRepairer;
};

} // namespace Test
} // namespace Av
} // namespace OpenHome


using namespace OpenHome;
using namespace OpenHome::Av;
using namespace OpenHome::Av::Test;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Test;


// MockResendRequester

MockResendRequester::MockResendRequester(ITestPipeWritable& aTestPipe)
    : iTestPipe(aTestPipe)
{
}

void MockResendRequester::RequestResendSequences(const std::vector<const IResendRange*> aRanges)
{
    Bws<50> buf("MRR::ReqestResend");
    for (auto range : aRanges) {
        buf.Append(" ");
        Ascii::AppendDec(buf, range->Start());
        buf.Append("->");
        Ascii::AppendDec(buf, range->End());
    }
    iTestPipe.Write(buf);
}


// MockAudioSupply

MockAudioSupply::MockAudioSupply(ITestPipeWritable& aTestPipe)
    : iTestPipe(aTestPipe)
{
}

void MockAudioSupply::OutputAudio(const Brx& aAudio)
{
    ASSERT(aAudio.Bytes() > 0);
    Bws<50> buf("MAS::OutputAudio ");
    Ascii::AppendDec(buf, aAudio.Bytes());
    buf.Append(" ");
    buf.Append(aAudio);
    iTestPipe.Write(buf);
}


// MockRepairable

MockRepairable::MockRepairable(ITestPipeWritable& aTestPipe, MockRepairableAllocator& aAllocator, TUint aMaxBytes)
    : iTestPipe(aTestPipe)
    , iAllocator(aAllocator)
    , iData(aMaxBytes)
{
}

void MockRepairable::Set(TUint aFrame, TBool aResend, const Brx& aData)
{
    iFrame = aFrame;
    iResend = aResend;
    iData.Replace(aData);
}

TUint MockRepairable::Frame() const
{
    return iFrame;
}

TBool MockRepairable::Resend() const
{
    return iResend;
}

const Brx& MockRepairable::Data() const
{
    return iData;
}

void MockRepairable::Destroy()
{
    Bws<50> buf("MR::Destroy ");
    Ascii::AppendDec(buf, iFrame);
    iTestPipe.Write(buf);
    iAllocator.Deallocate(this);
}


// MockRepairableAllocator

MockRepairableAllocator::MockRepairableAllocator(ITestPipeWritable& aTestPipe, TUint aMaxRepairable, TUint aMaxBytes)
    : iFifo(aMaxRepairable)
{
    for (TUint i=0; i<aMaxRepairable; i++) {
        iFifo.Write(new MockRepairable(aTestPipe, *this, aMaxBytes));
    }
}

MockRepairableAllocator::~MockRepairableAllocator()
{
    ASSERT(iFifo.SlotsFree() == 0); // All repairables must have been returned.
    while (iFifo.SlotsUsed() > 0) {
        auto repairable = iFifo.Read();
        delete repairable;
    }
}

IRepairable* MockRepairableAllocator::Allocate(TUint aFrame, TBool aResend, const Brx& aData)
{
    auto repairable = iFifo.Read();
    repairable->Set(aFrame, aResend, aData);
    return repairable;
}

void MockRepairableAllocator::Deallocate(MockRepairable* aRepairable)
{
    aRepairable->Set(0, false, Brx::Empty());
    iFifo.Write(aRepairable);
}


// MockTimerRepairer

MockTimerRepairer::MockTimerRepairer(ITestPipeWritable& aTestPipe, Functor aCallback, const TChar* aId)
    : iTestPipe(aTestPipe)
    , iCallback(aCallback)
    , iId(aId)
{
}

const TChar* MockTimerRepairer::Id() const
{
    return iId;
}

void MockTimerRepairer::Fire()
{
    iCallback();
}

void MockTimerRepairer::FireIn(TUint /*aMs*/)
{
    Bws<64> buf("MT::FireIn ");
    buf.Append(iId);
    // Time parameter not reported.
    iTestPipe.Write(buf);
}

void MockTimerRepairer::Cancel()
{
    Bws<64> buf("MT::Cancel ");
    buf.Append(iId);
    iTestPipe.Write(buf);
}


// MockTimerFactoryRepairer

MockTimerFactoryRepairer::MockTimerFactoryRepairer(ITestPipeWritable& aTestPipe)
    : iTestPipe(aTestPipe)
{
}

void MockTimerFactoryRepairer::FireTimer(const TChar* aId)
{
    for (MockTimerRepairer& t : iTimers) {
        if (*aId == *t.Id()) {
            t.Fire();
            return;
        }
    }
    ASSERTS(); // No such timer.
}

ITimer* MockTimerFactoryRepairer::CreateTimer(Functor aCallback, const TChar* aId)
{
    // The ITimer that is returned from here must not be destroyed until all
    // calls to FireTimer() have been made.
    MockTimerRepairer* timer = new MockTimerRepairer(iTestPipe, aCallback, aId);
    iTimers.push_back(*timer);
    return timer;
}


// SuiteRaopResend


SuiteRaopResend::SuiteRaopResend(Environment& aEnv)
    : SuiteUnitTest("SuiteRaopResend")
    , iEnv(aEnv)
{
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestNoDropouts), "TestNoDropouts");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendOnePacket), "TestResendOnePacket");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendMultiplePackets), "TestResendMultiplePackets");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendMulitpleRanges), "TestResendMulitpleRanges");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendBeyondMultipleRangeLimit), "TestResendBeyondMultipleRangeLimit");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestMultipleResendRecover), "TestMultipleResendRecover");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendRequest), "TestResendRequest");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendPacketBufferOverflowFirst), "TestResendPacketBufferOverflowFirst");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendPacketBufferOverflowMiddle), "TestResendPacketBufferOverflowMiddle");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendPacketBufferOverflowLast), "TestResendPacketBufferOverflowLast");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendBufferOverflowRecover), "TestResendBufferOverflowRecover");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendPacketsOutOfOrder), "TestResendPacketsOutOfOrder");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestDropPacketWhileAwaitingResend), "TestDropPacketWhileAwaitingResend");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestResendPacketsAlreadySeen), "TestResendPacketsAlreadySeen");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestStreamReset), "TestStreamReset");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestStreamResetResendPending), "TestStreamResetResendPending");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestDropAudio), "TestDropAudio");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestSequenceNumberWrapping), "TestSequenceNumberWrapping");
    AddTest(MakeFunctor(*this, &SuiteRaopResend::TestSequenceNumberWrappingDuringRepair), "TestSequenceNumberWrappingDuringRepair");
}

void SuiteRaopResend::Setup()
{
    iTestPipe = new TestPipeDynamic(kMaxTestPipeMessages);
    iResendRequester = new MockResendRequester(*iTestPipe);
    iAudioSupply = new MockAudioSupply(*iTestPipe);
    iTimerFactory = new MockTimerFactoryRepairer(*iTestPipe);
    // Repair buffer stashes first discontinuity frame in a pointer, then proceeds to fill a buffer of kMaxFrameBytes.
    // So, need kMaxFrames+2 to overflow repair buffer.
    iAllocator = new MockRepairableAllocator(*iTestPipe, kMaxFrames+2, kMaxFrameBytes);
    iRepairer = new Repairer<kMaxFrames>(iEnv, *iResendRequester, *iAudioSupply, *iTimerFactory);
}

void SuiteRaopResend::TearDown()
{
    delete iRepairer;
    delete iAllocator;
    delete iTimerFactory;
    delete iAudioSupply;
    delete iResendRequester;
    delete iTestPipe;
}

void SuiteRaopResend::TestNoDropouts()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    iRepairer->OutputAudio(*iAllocator->Allocate(1, false, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
}

void SuiteRaopResend::TestResendOnePacket()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    // Expect retry logic to kick in.
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Allow repairer to output resend request.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, deliver expected packet...
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));

    // FIXME
    // Don't expect timer to be cancelled, as could still be requesting other missing ranges...
    // ...but, would we expect it to be cancelled if the repair buffer was emptied (i.e., after the next packet was output, which is the only packet queued)?
    // ... followed by next that was buffered

    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));

    // Now, resume normal sequence.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));

    // Fire timer again. Should have no effect as no missing packets.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendMultiplePackets()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a couple of packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    // Expect retry logic to kick in.
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Allow repairer to output resend request.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, deliver expected packets...
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    iRepairer->OutputAudio(*iAllocator->Allocate(2, true, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));

    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));

    // Now, resume normal sequence.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendMulitpleRanges()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a couple of packets.
    // Have a couple of contiguous packets to ensure resend algorithm skips over these.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Miss more packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));

    // Allow repairer to output resend request.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->2 5->5")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in the missing packets, which should flush out the buffered packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    iRepairer->OutputAudio(*iAllocator->Allocate(2, true, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));

    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendBeyondMultipleRangeLimit()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Miss another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    // Miss another packet.
    // Can only fit (at most) kMaxFrames/2 resend packet in repair buffer
    // so packet 5 won't be in initial resend request.
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));

    // Allow repairer to output resend request.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1 3->3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in the missing packets, which should flush out the buffered packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    iRepairer->OutputAudio(*iAllocator->Allocate(3, true, Brn("3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));

    // Now fire timer to allow request for final missing packet.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 5->5")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    iRepairer->OutputAudio(*iAllocator->Allocate(5, true, Brn("5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestMultipleResendRecover()
{
    // Test that goes through a few recovery sequences.

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a couple of packet sequences.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));

    // Allow timer to fire.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->2 4->4")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Resend only the first missing packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));

    // Pass in another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));

    // Fire timer again.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 2->2 4->4")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in first missing packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, true, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));

    // Pass in another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));

    // Send in last missing packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, true, Brn("4")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));

    // Allow timer to fire again. Nothing should happen as no more missing packets.
    iTimerFactory->FireTimer("Repairer");

    // Send in more packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(8, false, Brn("8")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 8")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 8")));
    iRepairer->OutputAudio(*iAllocator->Allocate(9, false, Brn("9")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 9")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 9")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(11, false, Brn("11")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(12, false, Brn("12")));
    // Allow timer to fire.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 10->10")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // More packets arrive before resend request satisfied.
    iRepairer->OutputAudio(*iAllocator->Allocate(13, false, Brn("13")));
    iRepairer->OutputAudio(*iAllocator->Allocate(14, false, Brn("14")));
    // Resent packet arrives.
    iRepairer->OutputAudio(*iAllocator->Allocate(10, true, Brn("10")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 10")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 10")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 11")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 11")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 12")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 12")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 13")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 13")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 14")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 14")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendRequest()
{
    // Test resend requests are repeated if resend packets not received.
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send another packet in.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));

    // Fire timer again. Resend request should be made again as packet still hasn't arrived.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in missed packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendPacketBufferOverflowFirst()
{
    // An initial resend packet arrives, but will cause buffer to overflow.
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a couple of packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fill buffer with packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));
    iRepairer->OutputAudio(*iAllocator->Allocate(8, false, Brn("8")));

    // Receive the first packet being waited on. Should cause overflow.
    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(2, true, Brn("2"))), RepairerBufferFull);
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 8")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendPacketBufferOverflowMiddle()
{
    // A resend request has arrived for somewhere in middle of repair buffer, but subsequent frames have already arrived and filled buffer.

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Miss another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));

    // Fire timer again.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1 3->3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, send in some more packets to fill buffer.
    // So, have a packet missing at start and middle of repair buffer.
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));
    iRepairer->OutputAudio(*iAllocator->Allocate(8, false, Brn("8")));

    // Now, send in packet that was missing from middle of sequence (first packet still hasn't arrived).
    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(3, true, Brn("3"))), RepairerBufferFull);
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 8")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendPacketBufferOverflowLast()
{
    // Packet is missed and packets are pushed in at end of repair buffer until buffer overflows.

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in packets that should be appended to end of buffer until buffer overflows.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));

    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(8, false, Brn("8"))), RepairerBufferFull);
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 8")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendBufferOverflowRecover()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in packets that should be appended to end of buffer until buffer overflows.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));

    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(8, false, Brn("8"))), RepairerBufferFull);
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 8")));

    // Now, continue packet sequence. Should be passed on as normal.
    iRepairer->OutputAudio(*iAllocator->Allocate(9, false, Brn("9")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 9")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 9")));
    iRepairer->OutputAudio(*iAllocator->Allocate(10, false, Brn("10")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 2 10")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 10")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendPacketsOutOfOrder()
{
    // Miss a couple of packets and resend out of order.

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Miss another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));

    // Fire timer again.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1 3->3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, send in the packets out of order.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, true, Brn("3")));
    TEST(iTestPipe->ExpectEmpty());
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestDropPacketWhileAwaitingResend()
{
    // Drop a packet between putting out a resend request and receiving the
    // resent packet (and before resend timer fires again)

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Send in a couple more packets.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, false, Brn("3")));
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));
    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(6, false, Brn("6")));
    TEST(iTestPipe->ExpectEmpty());

    // Now, receive resent packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));

    // Send in another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(7, false, Brn("7")));

    // Fire timer again, should still be repairing.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 5->5")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, send in requested packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(5, true, Brn("5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 6")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 6")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 7")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 7")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestResendPacketsAlreadySeen()
{
    // Miss a couple of packets and have a duplicate resent.

    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));

    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Miss another packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(4, false, Brn("4")));

    // Fire timer again.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1 3->3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Act like first request wasn't answered and fire timer again.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1 3->3")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, send repair packets in.
    // Pretend race condition where packet 3 was actually sent after first request but just didn't arrive in time.
    iRepairer->OutputAudio(*iAllocator->Allocate(3, true, Brn("3")));
    TEST(iTestPipe->ExpectEmpty());
    // Then, both packets were sent successfully after second request. Duplicate packet 3 should have no effect.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, true, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 4")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 4")));
    iRepairer->OutputAudio(*iAllocator->Allocate(3, true, Brn("3")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 3")));  // Discard resend.

    // Continue sequence.
    iRepairer->OutputAudio(*iAllocator->Allocate(5, false, Brn("5")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 5")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 5")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestStreamReset()
{
    // Receiving a packet already seen but is not a resend.

    // Case 1: Normal sequence of packets (none missing) and stream is restarted.
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    iRepairer->OutputAudio(*iAllocator->Allocate(1, false, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    // Now, output packet with a seq no. already seen, but that is not a resend.
    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0"))), RepairerStreamRestarted);
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    TEST(iTestPipe->ExpectEmpty());

    // Continue new stream.
    // Retain ownership of msg after a RepairerStreamRestarted.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, false, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestStreamResetResendPending()
{
    // Case 2: Waiting on a missed packet when a stream is restarted.
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Send in packet with seq no. already seen, but that is not a resend.
    TEST_THROWS(iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0"))), RepairerStreamRestarted);
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    TEST(iTestPipe->ExpectEmpty());

    // Continue new stream.
    // Retain ownership of msg after a RepairerStreamRestarted.
    iRepairer->OutputAudio(*iAllocator->Allocate(1, false, Brn("1")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 1")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 1")));
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 2")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));
    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestDropAudio()
{
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(2, false, Brn("2")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 1->1")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));

    // Now, tell Repairer to drop audio.
    iRepairer->DropAudio();
    TEST(iTestPipe->Expect(Brn("MT::Cancel Repairer")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 2")));

    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestSequenceNumberWrapping()
{
    // RAOP sequence number is a 16-bit uint and wraps from 65535 to 0.
    // Check that repairer deals with that correctly and does not believe
    // there's been a dropout or a stream restart.

    iRepairer->OutputAudio(*iAllocator->Allocate(65535, false, Brn("65535")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 5 65535")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 65535")));
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
    TEST(iTestPipe->ExpectEmpty());
}

void SuiteRaopResend::TestSequenceNumberWrappingDuringRepair()
{
    // Test sequence number wrapping while repair is active.
    iRepairer->OutputAudio(*iAllocator->Allocate(65533, false, Brn("65533")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 5 65533")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 65533")));
    // Miss a packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(65535, false, Brn("65535")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Fire timer.
    iTimerFactory->FireTimer("Repairer");
    TEST(iTestPipe->Expect(Brn("MRR::ReqestResend 65534->65534")));
    TEST(iTestPipe->Expect(Brn("MT::FireIn Repairer")));
    // Send in another packet, which wraps sequence no.
    iRepairer->OutputAudio(*iAllocator->Allocate(0, false, Brn("0")));

    // Send in missing packet.
    iRepairer->OutputAudio(*iAllocator->Allocate(65534, false, Brn("65534")));
    // Missing packet should be output, allong with all others.
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 5 65534")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 65534")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 5 65535")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 65535")));
    TEST(iTestPipe->Expect(Brn("MAS::OutputAudio 1 0")));
    TEST(iTestPipe->Expect(Brn("MR::Destroy 0")));
}



void TestRaop(Environment& aEnv)
{
    Runner runner("RAOP tests\n");
    runner.Add(new SuiteRaopResend(aEnv));
    runner.Run();
}
