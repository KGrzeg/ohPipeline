#include <OpenHome/Av/Source.h>
#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Av/Product.h>
#include <OpenHome/Media/PipelineManager.h>
#include <OpenHome/Configuration/ConfigManager.h>
#include <OpenHome/PowerManager.h>
#include <OpenHome/Private/Ascii.h>

using namespace OpenHome;
using namespace OpenHome::Av;
using namespace OpenHome::Configuration;


// SourceBase

const TUint SourceBase::kConfigValSourceInvisible = 0;
const TUint SourceBase::kConfigValSourceVisible   = 1;
const Brn SourceBase::kKeySourceNamePrefix("Source.");
const Brn SourceBase::kKeySourceNameSuffix(".Name");
const Brn SourceBase::kKeySourceVisibleSuffix(".Visible");

void SourceBase::GetSourceNameKey(const Brx& aSystemName, Bwx& aBuf)
{ // static
    GetSourceKey(aSystemName, kKeySourceNameSuffix, aBuf);
}

void SourceBase::GetSourceVisibleKey(const Brx& aSystemName, Bwx& aBuf)
{ // static
    GetSourceKey(aSystemName, kKeySourceVisibleSuffix, aBuf);
}

void SourceBase::GetSourceKey(const Brx& aSystemName, const Brx& aSuffix, Bwx& aBuf)
{ // static
    aBuf.Replace(kKeySourceNamePrefix);
    aBuf.Append(aSystemName);
    aBuf.Append(aSuffix);
}

const Brx& SourceBase::SystemName() const
{
    return iSystemName;
}

const Brx& SourceBase::Type() const
{
    return iType;
}

void SourceBase::Name(Bwx& aBuf) const
{
    AutoMutex a(iLock);
    aBuf.Replace(iName);
}

TBool SourceBase::IsVisible() const
{
    return iVisible;
}

void SourceBase::Activate(TBool /*aAutoPlay*/, TBool /*aPrefetchAllowed*/)
{
    iActive = true;
}

void SourceBase::Deactivate()
{
    iActive = false;
}

void SourceBase::SetVisible(TBool aVisible)
{
    iVisible = aVisible;
}

SourceBase::SourceBase(const Brx& aSystemName, const TChar* aType, TBool aIsVisibleByDefault)
    : iActive(false)
    , iLock("SRCM")
    , iSystemName(aSystemName)
    , iType(aType)
    , iName(aSystemName)
    , iVisible(true)
    , iProduct(nullptr)
    , iConfigName(nullptr)
    , iConfigVisible(nullptr)
    , iConfigNameSubscriptionId(IConfigManager::kSubscriptionIdInvalid)
    , iConfigVisibleSubscriptionId(IConfigManager::kSubscriptionIdInvalid)
    , iConfigNameCreated(false)
    , iConfigVisibleCreated(false)
    , iIsVisibleByDefault(aIsVisibleByDefault)
{
}

SourceBase::~SourceBase()
{
    if (iConfigName != nullptr) {
        iConfigName->Unsubscribe(iConfigNameSubscriptionId);
        if (iConfigNameCreated) {
            delete iConfigName;
        }
    }
    if (iConfigVisible != nullptr) {
        iConfigVisible->Unsubscribe(iConfigVisibleSubscriptionId);
        if (iConfigVisibleCreated) {
            delete iConfigVisible;
        }
    }
}

TBool SourceBase::IsActive() const
{
    return iActive;
}

void SourceBase::ActivateIfNotActive()
{
    iActive = true;
    iProduct->ActivateIfNotActive(*this, true);
}

void SourceBase::ActivateIfNotActiveNoPrefetch()
{
    iActive = true;
    iProduct->ActivateIfNotActive(*this, false);
}

void SourceBase::Initialise(IProduct& aProduct, IConfigInitialiser& aConfigInit, IConfigManager& aConfigManagerReader, TUint /*aId*/)
{
    iProduct = &aProduct;

    Bws<kKeySourceNameMaxBytes> key;
    GetSourceNameKey(iSystemName, key);
    if (aConfigManagerReader.HasText(key)) {
        iConfigName = &aConfigManagerReader.GetText(key);
        iConfigNameCreated = false;
    } else {
        iConfigName = new ConfigText(aConfigInit, key, ISource::kMaxSourceNameBytes, iName);
        iConfigNameCreated = true;
    }
    iConfigNameSubscriptionId = iConfigName->Subscribe(MakeFunctorConfigText(*this, &SourceBase::NameChanged));

    GetSourceVisibleKey(iSystemName, key);
    if (aConfigManagerReader.HasChoice(key)) {
        iConfigVisible = &aConfigManagerReader.GetChoice(key);
        iConfigVisibleCreated = false;
    }
    else {
        std::vector<TUint> choices;
        choices.push_back(kConfigValSourceInvisible);
        choices.push_back(kConfigValSourceVisible);
        if (iIsVisibleByDefault) {
            iConfigVisible = new ConfigChoice(aConfigInit, key, choices, kConfigValSourceVisible);
        }
        else {
            iConfigVisible = new ConfigChoice(aConfigInit, key, choices, kConfigValSourceInvisible);
        }
        iConfigVisibleCreated = true;
    }
    iConfigVisibleSubscriptionId = iConfigVisible->Subscribe(MakeFunctorConfigChoice(*this, &SourceBase::VisibleChanged));

}

void SourceBase::NameChanged(KeyValuePair<const Brx&>& aName)
{
    iLock.Wait();
    iName.Replace(aName.Value());
    iLock.Signal();
    iProduct->NotifySourceChanged(*this);
}

void SourceBase::VisibleChanged(Configuration::KeyValuePair<TUint>& aKvp)
{
    iLock.Wait();
    iVisible = (aKvp.Value() == kConfigValSourceVisible);
    iLock.Signal();
    iProduct->NotifySourceChanged(*this);
}


// Source

Source::Source(const Brx& aSystemName, const TChar* aType, Media::PipelineManager& aPipeline, TBool aIsVisibleByDefault)
    : SourceBase(aSystemName, aType, aIsVisibleByDefault)
    , iPipeline(aPipeline)
    , iLockActivation("SRC ")
{
}

void Source::DoPlay()
{
    ActivateIfNotActive();
    iPipeline.Play();
}

void Source::EnsureActiveNoPrefetch()
{
    AutoMutex _(iLockActivation);
    ActivateIfNotActiveNoPrefetch();
}
