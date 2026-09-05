// Copyright Narrative Tools 2025.


#include "Settings/NarrativeInputSettings.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "Engine/Engine.h"
#include "UserSettings/EnhancedPlayerMappableKeyProfile.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/CustomVersion.h"
#include "UObject/ObjectVersion.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include "Misc/ScopeExit.h"

UNarrativeInputSettings::UNarrativeInputSettings()
{
	AimSensitivity = 1.f; 
}

void UNarrativeInputSettings::SetAimSensitivity(const float NewAimSensitivity)
{
	AimSensitivity = FMath::IsFinite(NewAimSensitivity) ? FMath::Clamp(NewAimSensitivity, 0.05f, 10.f) : 1.f;
	SaveSettings();
}

float UNarrativeInputSettings::GetAimSensitivity() const
{
	return FMath::IsFinite(AimSensitivity) ? FMath::Clamp(AimSensitivity, .05f, 10.f) : 1.f;
}

void UNarrativeInputSettings::SetInvertVertical(const bool NewInvertVertical)
{
	bInvertVertical = NewInvertVertical;
	SaveSettings();
}

bool UNarrativeInputSettings::GetInvertVertical() const
{
	return bInvertVertical;
}

void UNarrativeInputSettings::SetInvertHorizontal(const bool NewInvertHorizontal)
{
	bInvertHorizontal = NewInvertHorizontal;
	SaveSettings();
}

bool UNarrativeInputSettings::GetInvertHorizontal() const
{
	return bInvertHorizontal;
}

void UNarrativeInputSettings::SetCameraSensitivity(float Value)
{ CameraSensitivity = FMath::IsFinite(Value) ? FMath::Clamp(Value, .05f, 10.f) : 1.f; SaveSettings(); }
float UNarrativeInputSettings::GetCameraSensitivity() const
{ return FMath::IsFinite(CameraSensitivity) ? FMath::Clamp(CameraSensitivity, .05f, 10.f) : 1.f; }
void UNarrativeInputSettings::SetGamepadDeadZone(float Value)
{ GamepadDeadZone = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, .9f) : 0.f; SaveSettings(); }
float UNarrativeInputSettings::GetGamepadDeadZone() const
{ return FMath::IsFinite(GamepadDeadZone) ? FMath::Clamp(GamepadDeadZone, 0.f, .9f) : 0.f; }
void UNarrativeInputSettings::SetGamepadAccelerationSeconds(float Value)
{ GamepadAccelerationSeconds = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 2.f) : 0.f; SaveSettings(); }
float UNarrativeInputSettings::GetGamepadAccelerationSeconds() const
{ return FMath::IsFinite(GamepadAccelerationSeconds) ? FMath::Clamp(GamepadAccelerationSeconds, 0.f, 2.f) : 0.f; }

// Account storage is supplied by the game's existing settings authority. Other Narrative
// games keep the plugin's original engine save behavior through neutral base defaults.
void UNarrativeInputSettings::Initialize(ULocalPlayer* LocalPlayer)
{
    { TGuardValue<bool> Applying(bApplyingAccountProfile,true); Super::Initialize(LocalPlayer); }
    if (auto* Settings=GEngine?Cast<UNarrativeGameUserSettings>(GEngine->GetGameUserSettings()):nullptr)
    { if(Settings->UsesVerifiedAccountPreferences()) { Settings->RegisterOwnedInputSettings(this); } }
}
void UNarrativeInputSettings::SaveSettings()
{
    if(bApplyingAccountProfile) { return; }
    if(auto* Settings=GEngine?Cast<UNarrativeGameUserSettings>(GEngine->GetGameUserSettings()):nullptr)
    { if(Settings->UsesVerifiedAccountPreferences()) { Settings->PersistOwnedInputSettings(this); return; } }
    Super::SaveSettings();
}
void UNarrativeInputSettings::AsyncSaveSettings()
{
    // The account authority uses verified alternating banks. Never dispatch an unfenced
    // engine async write to its hardcoded slot, even if account ownership changes later.
    if(auto* Settings=GEngine?Cast<UNarrativeGameUserSettings>(GEngine->GetGameUserSettings()):nullptr)
    { if(Settings->UsesVerifiedAccountPreferences()) { SaveSettings(); return; } }
    Super::AsyncSaveSettings();
}
namespace
{
    constexpr uint32 InputProfileMagic=0x534F5649;
    constexpr int32 MaximumInputProfileBytes=262144;
    struct FInputProfileVersions
    {
        FPackageFileVersion Package;
        int32 Licensee=0;
        FCustomVersionContainer Custom;
    };
    bool UnpackInputProfile(const TArray<uint8>& Bytes,FInputProfileVersions& Versions,TArray<uint8>& Body)
    {
        if(Bytes.Num()<24 || Bytes.Num()>MaximumInputProfileBytes) { return false; }
        FMemoryReader Header(Bytes,true); uint32 Magic=0,Schema=0,Count=0,Size=0;
        Header<<Magic<<Schema<<Versions.Package<<Versions.Licensee<<Count;
        if(Header.IsError() || Magic!=InputProfileMagic || Schema!=1 || Count>128 || int64(Count)*20+4>Header.TotalSize()-Header.Tell()) { return false; }
        for(uint32 Index=0;Index<Count;++Index)
        {
            FGuid Key; int32 Version=0; Header<<Key<<Version;
            if(!Key.IsValid() || Versions.Custom.GetVersion(Key)) { return false; }
            Versions.Custom.SetVersion(Key,Version,NAME_None);
        }
        Header<<Size;
        if(Header.IsError() || Size==0 || Size>MaximumInputProfileBytes || int64(Size)!=Header.TotalSize()-Header.Tell()) { return false; }
        Body.Reset(); Body.Append(Bytes.GetData()+Header.Tell(),Size); return true;
    }
}
bool UNarrativeInputSettings::CaptureAccountProfile(TArray<uint8>& Bytes)
{
    Bytes.Reset(); TArray<uint8> Body; FMemoryWriter Memory(Body,true);
    FObjectAndNameAsStringProxyArchive Writer(Memory,false); Writer.ArIsSaveGame=true; Writer.ArNoDelta=true; Writer.ArMaxSerializeSize=MaximumInputProfileBytes;
    Serialize(Writer);
    const auto& Versions=Writer.GetCustomVersions().GetAllVersions();
    if(Writer.IsError() || Body.Num()>MaximumInputProfileBytes || Versions.Num()>128) { return false; }
    FMemoryWriter Header(Bytes,true); uint32 Magic=InputProfileMagic,Schema=1,Count=Versions.Num(),Size=Body.Num();
    auto Package=Writer.UEVer(); int32 Licensee=Writer.LicenseeUEVer();
    Header<<Magic<<Schema<<Package<<Licensee<<Count;
    for(const auto& Item:Versions) { FGuid Key=Item.Key; int32 Version=Item.Version; Header<<Key<<Version; }
    Header<<Size; if(Size) { Header.Serialize(Body.GetData(),Size); }
    return !Header.IsError() && Bytes.Num()<=MaximumInputProfileBytes;
}
bool UNarrativeInputSettings::ApplyAccountProfile(const TArray<uint8>& Bytes)
{
    if(Bytes.Num()>MaximumInputProfileBytes) { return false; }
    // Preflight the bounded native-version frame before touching any live registration.
    FInputProfileVersions Versions; TArray<uint8> Body;
    if(!Bytes.IsEmpty() && !UnpackInputProfile(Bytes,Versions,Body)) { return false; }
    const uint64 Generation=++AccountProfileApplyGeneration;
    TGuardValue<bool> Applying(bApplyingAccountProfile,true);
    if(AccountProfileApplyDepth==0)
    {
        AccountProfileContextBaseline.Reset();
        for(const auto& Context:GetRegisteredInputMappingContexts())
        { const UInputMappingContext* Mapping=Context; AccountProfileContextBaseline.Add(const_cast<UInputMappingContext*>(Mapping)); }
    }
    ++AccountProfileApplyDepth;
    const auto Contexts=AccountProfileContextBaseline;
    bool bRestoredContexts=false;
    ON_SCOPE_EXIT
    {
        // A rejected payload must not erase the registrations needed to reconstruct
        // defaults on the next attempt. Never restore across a reentrant owner change.
        if(!bRestoredContexts && Generation==AccountProfileApplyGeneration)
        { for(const auto& Context:Contexts) { RegisterInputMappingContext(Context); if(Generation!=AccountProfileApplyGeneration) { break; } } }
        // Reentrant owner replacement inherits the entire outer registration set,
        // even when the callback arrives halfway through unregister/re-register.
        if(--AccountProfileApplyDepth==0) { AccountProfileContextBaseline.Reset(); }
    };
    for(const auto& Context:Contexts) { UnregisterInputMappingContext(Context); if(Generation!=AccountProfileApplyGeneration) { return false; } }
    // The engine serializes key profiles separately. Retire every existing profile map
    // first so loading B cannot leave A's named profiles available to the settings UI.
    // Match the reflected value type rather than a version-specific private field name.
    for(TFieldIterator<FMapProperty> It(GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        const auto* Value=CastField<FObjectPropertyBase>(It->ValueProp);
        if(Value && Value->PropertyClass && Value->PropertyClass->IsChildOf(UEnhancedPlayerMappableKeyProfile::StaticClass()))
        { FScriptMapHelper Map(*It,It->ContainerPtrToValuePtr<void>(this)); Map.EmptyValues(); }
    }
    TArray<uint8> Defaults;
    if(Bytes.IsEmpty())
    {
        TStrongObjectPtr<UNarrativeInputSettings> Fresh(NewObject<UNarrativeInputSettings>());
        Fresh->CameraSensitivity=1.f; Fresh->AimSensitivity=1.f; Fresh->GamepadDeadZone=0.f;
        Fresh->GamepadAccelerationSeconds=0.f; Fresh->bInvertHorizontal=false; Fresh->bInvertVertical=false;
        Fresh->bApplyingAccountProfile=true;
        Fresh->UEnhancedInputUserSettings::Initialize(GetLocalPlayer());
        if(!Fresh->CaptureAccountProfile(Defaults) || !UnpackInputProfile(Defaults,Versions,Body)) { return false; }
    }
    FMemoryReader Memory(Body,true);
    FObjectAndNameAsStringProxyArchive Reader(Memory,false); Reader.ArIsSaveGame=true; Reader.ArNoDelta=true; Reader.ArMaxSerializeSize=MaximumInputProfileBytes;
    Reader.SetUEVer(Versions.Package); Reader.SetLicenseeUEVer(Versions.Licensee); Reader.SetCustomVersions(Versions.Custom);
    Serialize(Reader);
    if(Reader.IsError() || !Memory.AtEnd() || Generation!=AccountProfileApplyGeneration) { return false; }
    for(const auto& Context:Contexts) { RegisterInputMappingContext(Context); if(Generation!=AccountProfileApplyGeneration) { return false; } }
    bRestoredContexts=true; ApplySettings();
    return Generation==AccountProfileApplyGeneration;
}
