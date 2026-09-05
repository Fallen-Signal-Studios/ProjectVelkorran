// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Settings/SovGameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/NarrativeInputSettings.h"
#include "Engine/LocalPlayer.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <type_traits>

namespace
{
constexpr uint32 ProfileMagic = 0x534F5650;
constexpr uint32 ProfileVersion = 1;
constexpr int32 MaximumProfileBytes = 266240;
struct FPreferences
{
    FSovUserSettingsSnapshot Snapshot;
    FSovHapticSettings Haptic;
    TArray<uint8> InputProfile;
    bool Completed = false, Diagnostics = false, Setup = false;
    float OverallAudioVolume=1.f, DialogueAudioVolume=1.f, UIAudioVolume=1.f, SFXAudioVolume=1.f;
    float MusicAudioVolume=.3f, AmbienceAudioVolume=1.f, TinnitusAudioVolume=1.f;
    ENarrativeAudioDynamicRange AudioDynamicRange=ENarrativeAudioDynamicRange::Full;
    bool bEnableBloom=true, bEnableMotionBlur=true, bCrouchToggles=true, bInventoryWantsTile=true;
    ENarrativeGameplayDifficulty GameplayDifficulty=ENarrativeGameplayDifficulty::Medium;
    ENarrativeSubtitleLevel SubtitleLevel=ENarrativeSubtitleLevel::Enabled;
    float FieldOfView=90.f, WeaponFieldOfView=90.f;
};
template<typename T> void Scalar(FArchive& Ar,T& Value)
{
    if constexpr (std::is_enum_v<T>) { uint8 Raw=static_cast<uint8>(Value); Ar<<Raw; if(Ar.IsLoading()) { Value=static_cast<T>(Raw); } }
    else if constexpr (std::is_same_v<T,bool>) { uint8 Raw=Value?1:0; Ar<<Raw; if(Ar.IsLoading()) { if(Raw>1) { Ar.SetError(); } Value=Raw!=0; } }
    else { Ar<<Value; }
}
void SerializePreferences(FArchive& Ar,FPreferences& P)
{
    Scalar(Ar,P.Snapshot.Preset);
    Scalar(Ar,P.Snapshot.IncomingDamageScale);
    Scalar(Ar,P.Snapshot.EnemyRecoveryScale);
    Scalar(Ar,P.Snapshot.bAllowCompanionRescue);
    Scalar(Ar,P.Snapshot.DefenseWindowScale);
    Scalar(Ar,P.Snapshot.ExertionCostScale);
    Scalar(Ar,P.Snapshot.InputBufferAssistanceSeconds);
    Scalar(Ar,P.Snapshot.MeleeAimAssistStrength);
    Scalar(Ar,P.Snapshot.RangedAimAssistStrength);
    Scalar(Ar,P.Snapshot.InteractionHoldScale);
    Scalar(Ar,P.Snapshot.bTapInteractions);
    Scalar(Ar,P.Snapshot.bToggleAim);
    Scalar(Ar,P.Snapshot.bToggleGuard);
    Scalar(Ar,P.Snapshot.bToggleSprint);
    Scalar(Ar,P.Snapshot.bAutomaticSprint);
    Scalar(Ar,P.Snapshot.bAimSnap);
    Scalar(Ar,P.Snapshot.bProjectileLead);
    Scalar(Ar,P.Snapshot.bToggleAbilityModifier);
    Scalar(Ar,P.Snapshot.AutoCameraStrength);
    Scalar(Ar,P.Snapshot.bDisableCameraShake);
    Scalar(Ar,P.Snapshot.bReduceLensEffects);
    Scalar(Ar,P.Snapshot.bReduceCorruptionEffects);
    Scalar(Ar,P.Snapshot.UIScale);
    Scalar(Ar,P.Snapshot.SubtitleScale);
    Scalar(Ar,P.Snapshot.bSubtitles);
    Scalar(Ar,P.Snapshot.bClosedCaptions);
    Scalar(Ar,P.Snapshot.SubtitleBackgroundOpacity);
    Scalar(Ar,P.Snapshot.bSubtitleSpeakerNames);
    Scalar(Ar,P.Snapshot.bSubtitleDirections);
    Scalar(Ar,P.Snapshot.SubtitleCharactersPerLine);
    Scalar(Ar,P.Snapshot.SubtitleMaximumLines);
    Scalar(Ar,P.Snapshot.bHighContrastHUD);
    Scalar(Ar,P.Snapshot.ColorVisionPreset);
    Scalar(Ar,P.Snapshot.bOverrideTeamColor);
    Scalar(Ar,P.Snapshot.TeamColor);
    Scalar(Ar,P.Snapshot.bOverrideThreatColor);
    Scalar(Ar,P.Snapshot.ThreatColor);
    Scalar(Ar,P.Snapshot.bInteractableOutlines);
    Scalar(Ar,P.Snapshot.bWeakPointOutlines);
    Scalar(Ar,P.Snapshot.OutlineThickness);
    Scalar(Ar,P.Snapshot.bNavigationContrast);
    Scalar(Ar,P.Snapshot.bNavigationPulse);
    Scalar(Ar,P.Snapshot.bMenuNavigationWrap);
    Scalar(Ar,P.Snapshot.bMenuNarration);
    Scalar(Ar,P.Snapshot.DialoguePressureMode);
    Scalar(Ar,P.Snapshot.DialogueMinimumReadSeconds);
    Scalar(Ar,P.Snapshot.DialoguePressureExtension);
    Scalar(Ar,P.Snapshot.ControllerAudioVolume);
    Scalar(Ar,P.Haptic.Master);
    Scalar(Ar,P.Haptic.Combat);
    Scalar(Ar,P.Haptic.Interaction);
    Scalar(Ar,P.Haptic.Cinematic);
    Scalar(Ar,P.Haptic.Ambience);
    Scalar(Ar,P.Haptic.UI);
    Scalar(Ar,P.Completed);
    Scalar(Ar,P.Diagnostics);
    Scalar(Ar,P.Setup);
    Scalar(Ar,P.OverallAudioVolume);
    Scalar(Ar,P.DialogueAudioVolume);
    Scalar(Ar,P.UIAudioVolume);
    Scalar(Ar,P.SFXAudioVolume);
    Scalar(Ar,P.MusicAudioVolume);
    Scalar(Ar,P.AmbienceAudioVolume);
    Scalar(Ar,P.TinnitusAudioVolume);
    Scalar(Ar,P.AudioDynamicRange);
    Scalar(Ar,P.bEnableBloom);
    Scalar(Ar,P.bEnableMotionBlur);
    Scalar(Ar,P.bCrouchToggles);
    Scalar(Ar,P.bInventoryWantsTile);
    Scalar(Ar,P.GameplayDifficulty);
    Scalar(Ar,P.SubtitleLevel);
    Scalar(Ar,P.FieldOfView);
    Scalar(Ar,P.WeaponFieldOfView);
    uint32 InputBytes=P.InputProfile.Num(); Ar<<InputBytes;
    if(InputBytes>262144 || (Ar.IsLoading() && int64(InputBytes)>Ar.TotalSize()-Ar.Tell())) { Ar.SetError(); return; }
    if(Ar.IsLoading()) { P.InputProfile.SetNumUninitialized(InputBytes); }
    if(InputBytes) { Ar.Serialize(P.InputProfile.GetData(),InputBytes); }
}
bool Valid(const FPreferences& P)
{
    FString Error;
    if (!USovGameUserSettings::ValidateSnapshot(P.Snapshot,P.Completed,Error) || !P.Haptic.IsValid()
        || uint8(P.AudioDynamicRange)>2 || uint8(P.GameplayDifficulty)>3 || uint8(P.SubtitleLevel)>2
        || !FMath::IsFinite(P.FieldOfView) || P.FieldOfView<30.f || P.FieldOfView>170.f
        || !FMath::IsFinite(P.WeaponFieldOfView) || P.WeaponFieldOfView<30.f || P.WeaponFieldOfView>170.f) { return false; }
    for(float Value : {P.OverallAudioVolume,P.DialogueAudioVolume,P.UIAudioVolume,P.SFXAudioVolume,P.MusicAudioVolume,P.AmbienceAudioVolume,P.TinnitusAudioVolume})
    { if(!FMath::IsFinite(Value) || Value<0.f || Value>1.f) { return false; } }
    return true;
}
FString ProfileSlot(const FString& Owner,int32 Bank)
{ return FString::Printf(TEXT("SovPreferences_%s_%d"),*FMD5::HashAnsiString(*Owner),Bank); }
void DigestOwner(const FString& Owner,uint8 (&Digest)[20])
{ FTCHARToUTF8 Utf8(*Owner); FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Digest); }
bool Unpack(const TArray<uint8>& Bytes,const FString& Owner,uint64& Generation,TArray<uint8>& Payload)
{
    constexpr int32 HeaderSize=4+4+8+4+20;
    if(Bytes.Num()<=HeaderSize || Bytes.Num()>MaximumProfileBytes) { return false; }
    FMemoryReader Reader(Bytes,true); uint32 Magic=0,Version=0,CRC=0; uint8 Expected[20],Stored[20];
    DigestOwner(Owner,Expected); Reader<<Magic<<Version<<Generation<<CRC; Reader.Serialize(Stored,20);
    if(Reader.IsError() || Magic!=ProfileMagic || Version!=ProfileVersion || Generation==0 || FMemory::Memcmp(Expected,Stored,20)!=0
        || FCrc::MemCrc32(Bytes.GetData()+HeaderSize,Bytes.Num()-HeaderSize)!=CRC) { return false; }
    Payload.Reset(); Payload.Append(Bytes.GetData()+HeaderSize,Bytes.Num()-HeaderSize);
    FPreferences P; FMemoryReader Data(Payload,true); SerializePreferences(Data,P);
    return !Data.IsError() && Data.AtEnd() && Valid(P);
}
}

void USovGameUserSettings::CaptureAccountPreferences(TArray<uint8>& Bytes) const
{
    FPreferences P; P.Snapshot=Settings; P.Haptic=HapticSettings; P.InputProfile=AccountInputProfile;
    P.Completed=bCampaignCompleted; P.Diagnostics=bLocalDiagnosticsEnabled; P.Setup=bAccessibilitySetupCompleted;
    P.OverallAudioVolume=OverallAudioVolume;
    P.DialogueAudioVolume=DialogueAudioVolume;
    P.UIAudioVolume=UIAudioVolume;
    P.SFXAudioVolume=SFXAudioVolume;
    P.MusicAudioVolume=MusicAudioVolume;
    P.AmbienceAudioVolume=AmbienceAudioVolume;
    P.TinnitusAudioVolume=TinnitusAudioVolume;
    P.AudioDynamicRange=AudioDynamicRange;
    P.bEnableBloom=bEnableBloom;
    P.bEnableMotionBlur=bEnableMotionBlur;
    P.bCrouchToggles=bCrouchToggles;
    P.bInventoryWantsTile=bInventoryWantsTile;
    P.GameplayDifficulty=GameplayDifficulty;
    P.SubtitleLevel=SubtitleLevel;
    P.FieldOfView=FieldOfView;
    P.WeaponFieldOfView=WeaponFieldOfView;
    Bytes.Reset(); FMemoryWriter Writer(Bytes,true); SerializePreferences(Writer,P);
}
bool USovGameUserSettings::ApplyAccountPreferences(const TArray<uint8>& Bytes)
{
    if(Bytes.IsEmpty() || Bytes.Num()>MaximumProfileBytes) { return false; }
    FPreferences P; FMemoryReader Reader(Bytes,true); SerializePreferences(Reader,P);
    if(Reader.IsError() || !Reader.AtEnd() || !Valid(P)) { return false; }
    Settings=P.Snapshot; HapticSettings=P.Haptic; AccountInputProfile=P.InputProfile;
    bCampaignCompleted=P.Completed; bLocalDiagnosticsEnabled=P.Diagnostics; bAccessibilitySetupCompleted=P.Setup;
    OverallAudioVolume=P.OverallAudioVolume;
    DialogueAudioVolume=P.DialogueAudioVolume;
    UIAudioVolume=P.UIAudioVolume;
    SFXAudioVolume=P.SFXAudioVolume;
    MusicAudioVolume=P.MusicAudioVolume;
    AmbienceAudioVolume=P.AmbienceAudioVolume;
    TinnitusAudioVolume=P.TinnitusAudioVolume;
    AudioDynamicRange=P.AudioDynamicRange;
    bEnableBloom=P.bEnableBloom;
    bEnableMotionBlur=P.bEnableMotionBlur;
    bCrouchToggles=P.bCrouchToggles;
    bInventoryWantsTile=P.bInventoryWantsTile;
    GameplayDifficulty=P.GameplayDifficulty;
    SubtitleLevel=P.SubtitleLevel;
    FieldOfView=P.FieldOfView;
    WeaponFieldOfView=P.WeaponFieldOfView;
    return true;
}
void USovGameUserSettings::ResetAccountPreferences()
{
    FPreferences Defaults; TArray<uint8> Bytes; FMemoryWriter Writer(Bytes,true); SerializePreferences(Writer,Defaults);
    ApplyAccountPreferences(Bytes); CommittedAccountPreferences=MoveTemp(Bytes); AccountSettingsFileGeneration=0;
}
bool USovGameUserSettings::IsVerifiedSettingsOwnerCurrent(const FString& Namespace,int32 Index,uint64 Generation) const
{
    return bAccountPreferencesManaged && !bAccountSettingsSuspended && !Namespace.IsEmpty() && Index>=0
        && AccountSettingsNamespace==Namespace && AccountSettingsUserIndex==Index && AccountSettingsOwnerGeneration==Generation;
}
void USovGameUserSettings::ObserveVerifiedSettingsOwner(const FString& Namespace,int32 UserIndex,bool bAuthorized)
{
    const bool Authorized=bAuthorized && !Namespace.IsEmpty() && UserIndex>=0;
    const FString Selected=Authorized?Namespace:FString(); const int32 Index=Authorized?UserIndex:INDEX_NONE;
    if(bAccountPreferencesManaged && AccountSettingsNamespace==Selected && AccountSettingsUserIndex==Index)
    { if(Authorized && !bAccountPreferencesLoaded && !bAccountSettingsSuspended) { if(bAccountSettingsIO) { ScheduleAccountPreferenceLoad(); } else { LoadAccountPreferences(); } } return; }
    bAccountPreferencesManaged=true; ++AccountSettingsOwnerGeneration;
    AccountSettingsNamespace=Selected; AccountSettingsUserIndex=Index; bAccountPreferencesLoaded=false;
    // Revoke/reset before external notifications or native I/O can pump callbacks.
    const uint64 Epoch=AccountSettingsOwnerGeneration;
    ResetAccountPreferences(); bLastPreferenceSaveSucceeded=false; ApplyOwnedInputProfile();
    if(Epoch!=AccountSettingsOwnerGeneration) { return; }
    if(Authorized && !bAccountSettingsSuspended) { if(bAccountSettingsIO) { ScheduleAccountPreferenceLoad(); } else { LoadAccountPreferences(); } }
    else { ApplySoundSettings(); if(Epoch!=AccountSettingsOwnerGeneration) { return; } OnUserSettingsChanged.Broadcast(Settings); if(Epoch==AccountSettingsOwnerGeneration) { OnHapticSettingsChanged.Broadcast(HapticSettings); } }
}
void USovGameUserSettings::SetPlatformSettingsSuspended(bool bSuspended)
{
    if(bAccountSettingsSuspended==bSuspended) { return; }
    bAccountSettingsSuspended=bSuspended; ++AccountSettingsOwnerGeneration;
    if(bSuspended && bAccountPreferencesManaged)
    {
        // A write whose verification was interrupted does not remain an optimistic
        // in-memory commit. Re-read verified banks when foreground ownership returns.
        bAccountPreferencesLoaded=false; bLastPreferenceSaveSucceeded=false;
        ApplyAccountPreferences(CommittedAccountPreferences);
    }
    if(!bSuspended && bAccountPreferencesManaged && !bAccountPreferencesLoaded)
    { if(bAccountSettingsIO) { ScheduleAccountPreferenceLoad(); } else { LoadAccountPreferences(); } }
}
void USovGameUserSettings::LoadAccountPreferences()
{
    const FString Owner=AccountSettingsNamespace; const int32 Index=AccountSettingsUserIndex; const uint64 Epoch=AccountSettingsOwnerGeneration;
    if(bAccountSettingsIO || !IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return; }
    TArray<uint8> Best; uint64 BestGeneration=0;
    {
        TGuardValue<bool> IO(bAccountSettingsIO,true);
        for(int32 Bank=0;Bank<2;++Bank)
        {
            TArray<uint8> Bytes,Payload; uint64 FileGeneration=0;
            const bool Read=ReadPreferenceBank(ProfileSlot(Owner,Bank),Index,Bytes);
            if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return; }
            if(Read && Unpack(Bytes,Owner,FileGeneration,Payload) && FileGeneration>BestGeneration)
            { Best=MoveTemp(Payload); BestGeneration=FileGeneration; }
        }
    }
    if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return; }
    if(!Best.IsEmpty()) { ApplyAccountPreferences(Best); CommittedAccountPreferences=MoveTemp(Best); AccountSettingsFileGeneration=BestGeneration; }
    bAccountPreferencesLoaded=true; const bool InputApplied=ApplyOwnedInputProfile();
    if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return; }
    bLastPreferenceSaveSucceeded=InputApplied;
    ApplySoundSettings();
    if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return; }
    OnUserSettingsChanged.Broadcast(Settings);
    if(IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { OnHapticSettingsChanged.Broadcast(HapticSettings); }
}
bool USovGameUserSettings::SaveAccountPreferences()
{
    const FString Owner=AccountSettingsNamespace; const int32 Index=AccountSettingsUserIndex; const uint64 Epoch=AccountSettingsOwnerGeneration;
    if(bAccountSettingsIO || !bAccountPreferencesLoaded || AccountSettingsFileGeneration==MAX_uint64 || !IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return false; }
    if(AccountInputSettings.IsValid())
    {
        UNarrativeInputSettings* Input=AccountInputSettings.Get(); ULocalPlayer* Local=Input->GetLocalPlayer();
        TArray<uint8> CapturedInput;
        if(!Local || IPlatformInputDeviceMapper::Get().GetUserIndexForPlatformUser(Local->GetPlatformUserId())!=Index
            || !Input->CaptureAccountProfile(CapturedInput)) { return false; }
        if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch) || AccountInputSettings.Get()!=Input) { return false; }
        AccountInputProfile=MoveTemp(CapturedInput);
    }
    if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return false; }
    TArray<uint8> Payload; CaptureAccountPreferences(Payload);
    FPreferences Check; FMemoryReader CheckReader(Payload,true); SerializePreferences(CheckReader,Check);
    if(CheckReader.IsError() || !Valid(Check)) { return false; }
    const uint64 Next=AccountSettingsFileGeneration+1; TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes,true); uint32 Magic=ProfileMagic,Version=ProfileVersion,CRC=FCrc::MemCrc32(Payload.GetData(),Payload.Num()); uint64 Generation=Next; uint8 Digest[20]; DigestOwner(Owner,Digest);
        Writer<<Magic<<Version<<Generation<<CRC; Writer.Serialize(Digest,20); Writer.Serialize(Payload.GetData(),Payload.Num());
    }
    bool Written=false;
    {
        TGuardValue<bool> IO(bAccountSettingsIO,true);
        if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return false; }
        Written=WritePreferenceBank(ProfileSlot(Owner,int32(Next%2)),Index,Bytes);
        if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return false; }
        if(Written)
        { TArray<uint8> Verification; Written=ReadPreferenceBank(ProfileSlot(Owner,int32(Next%2)),Index,Verification) && Verification==Bytes; }
    }
    if(!IsVerifiedSettingsOwnerCurrent(Owner,Index,Epoch)) { return false; }
    if(Written) { CommittedAccountPreferences=MoveTemp(Payload); AccountSettingsFileGeneration=Next; }
    else { ApplyAccountPreferences(CommittedAccountPreferences); }
    return Written;
}
void USovGameUserSettings::PersistDeviceSettings()
{
    if(!bAccountPreferencesManaged) { PersistSettings(); return; }
    const uint64 Epoch=AccountSettingsOwnerGeneration;
    TArray<uint8> Preferences; CaptureAccountPreferences(Preferences);
    FPreferences Defaults; TArray<uint8> DeviceDefaults; FMemoryWriter Writer(DeviceDefaults,true); SerializePreferences(Writer,Defaults);
    // The global config contains physical display configuration and neutral account defaults.
    // Unlike scoped value guards, an ownership change during persistence must not restore the old account.
    ApplyAccountPreferences(DeviceDefaults);
    PersistSettings();
    if(AccountSettingsOwnerGeneration==Epoch) { ApplyAccountPreferences(Preferences); }
}

void USovGameUserSettings::RegisterOwnedInputSettings(UNarrativeInputSettings* Input)
{
    if(!IsValid(Input) || AccountInputSettings.Get()==Input) { return; }
    AccountInputSettings=Input;
    ApplyOwnedInputProfile();
}
bool USovGameUserSettings::ApplyOwnedInputProfile()
{
    UNarrativeInputSettings* Input=AccountInputSettings.Get(); if(!Input) { return true; }
    const uint64 Epoch=AccountSettingsOwnerGeneration;
    ULocalPlayer* Local=Input->GetLocalPlayer();
    const bool OwnerMatches=Local && bAccountPreferencesLoaded && !AccountSettingsNamespace.IsEmpty()
        && IPlatformInputDeviceMapper::Get().GetUserIndexForPlatformUser(Local->GetPlatformUserId())==AccountSettingsUserIndex;
    const TArray<uint8> Empty;
    const TArray<uint8> Profile=OwnerMatches?AccountInputProfile:Empty;
    bool Applied=Input->ApplyAccountProfile(Profile);
    if(Epoch!=AccountSettingsOwnerGeneration || AccountInputSettings.Get()!=Input) { return false; }
    if(!Applied)
    {
        // A corrupt/version-incompatible remap never leaves another user's profiles active.
        AccountInputProfile.Reset(); Input->ApplyAccountProfile(Empty);
    }
    return Applied;
}
bool USovGameUserSettings::PersistOwnedInputSettings(UNarrativeInputSettings* Input)
{
    if(!IsValid(Input) || AccountInputSettings.Get()!=Input || !bAccountPreferencesLoaded)
    { if(Input) { const TArray<uint8> Empty; Input->ApplyAccountProfile(Empty); } return false; }
    const uint64 Epoch=AccountSettingsOwnerGeneration; SaveSettings();
    if(Epoch!=AccountSettingsOwnerGeneration) { return false; }
    if(!bLastPreferenceSaveSucceeded) { ApplyOwnedInputProfile(); }
    return Epoch==AccountSettingsOwnerGeneration && bLastPreferenceSaveSucceeded;
}

bool USovGameUserSettings::ReadPreferenceBank(const FString& Slot,int32 User,TArray<uint8>& Bytes)
{ return UGameplayStatics::LoadDataFromSlot(Bytes,Slot,User); }
bool USovGameUserSettings::WritePreferenceBank(const FString& Slot,int32 User,const TArray<uint8>& Bytes)
{ return UGameplayStatics::SaveDataToSlot(Bytes,Slot,User); }
void USovGameUserSettings::ScheduleAccountPreferenceLoad()
{
    if(AccountPreferenceLoadTicker.IsValid()) { return; }
    AccountPreferenceLoadTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
    {
        if(bAccountSettingsIO || bAccountSettingsSuspended) { return true; }
        if(!bAccountPreferencesLoaded && !AccountSettingsNamespace.IsEmpty()) { LoadAccountPreferences(); }
        if(!bAccountPreferencesLoaded && !AccountSettingsNamespace.IsEmpty()) { return true; }
        AccountPreferenceLoadTicker.Reset(); return false;
    }));
}
