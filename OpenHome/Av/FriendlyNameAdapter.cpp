
#include <OpenHome/Av/FriendlyNameAdapter.h>
#include <OpenHome/Private/Debug.h>


using namespace OpenHome;
using namespace OpenHome::Av;
using namespace OpenHome::Net;


FriendlyNameAttributeUpdater::FriendlyNameAttributeUpdater(
        IFriendlyNameObservable& aFriendlyNameObservable,
        DvDevice& aDvDevice,
        const Brx& aAppend)
    : iFriendlyNameObservable(aFriendlyNameObservable)
    , iDvDevice(aDvDevice)
    , iAppend(aAppend)
    , iLock("DNCL")
{
    iThread = new ThreadFunctor("UpnpNameChanger", MakeFunctor(*this, &FriendlyNameAttributeUpdater::Run));
    iThread->Start();

    iId = iFriendlyNameObservable.RegisterFriendlyNameObserver(
        MakeFunctorGeneric<const Brx&>(*this, &FriendlyNameAttributeUpdater::Observer));
}

FriendlyNameAttributeUpdater::FriendlyNameAttributeUpdater(
        IFriendlyNameObservable& aFriendlyNameObservable,
        DvDevice& aDvDevice)
    : FriendlyNameAttributeUpdater(aFriendlyNameObservable, aDvDevice, Brx::Empty())
{
}

FriendlyNameAttributeUpdater::~FriendlyNameAttributeUpdater()
{
    iFriendlyNameObservable.DeregisterFriendlyNameObserver(iId);
    delete iThread;
}

void FriendlyNameAttributeUpdater::Observer(const Brx& aNewFriendlyName)
{
    AutoMutex a(iLock);
    iFullName.Replace(aNewFriendlyName);
    iFullName.Append(iAppend);

    iThread->Signal();
}

void FriendlyNameAttributeUpdater::Run()
{
    try {
        for (;;) {
            iThread->Wait();
            AutoMutex a(iLock);
            iDvDevice.SetAttribute("Upnp.FriendlyName", iFullName.PtrZ());
        }
    }
    catch (ThreadKill&) {}
}
