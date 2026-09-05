// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Settings/SovGameUserSettings.h"
#include "Settings/NarrativeInputSettings.h"
#include "SovAccountPreferenceRepairTestFixtures.generated.h"

/** Only native I/O is substituted; schema, alternating banks, CRC and owner fences run unchanged. */
UCLASS(Transient, NotBlueprintable)
class USovAccountPreferenceRepairSettings : public USovGameUserSettings
{
    GENERATED_BODY()
public:
    TMap<FString,TArray<uint8>> Banks;
    TFunction<void()> DuringRead, DuringWrite;
    bool bRejectWrites=false;
    int32 Writes=0;
    float DeviceSavedUIScale=0.f;
protected:
    virtual bool ReadPreferenceBank(const FString& Slot,int32 User,TArray<uint8>& Bytes) override
    {
        auto Callback=MoveTemp(DuringRead); if(Callback) { Callback(); }
        if(const auto* Found=Banks.Find(FString::FromInt(User)+TEXT(":")+Slot)) { Bytes=*Found; return true; }
        return false;
    }
    virtual bool WritePreferenceBank(const FString& Slot,int32 User,const TArray<uint8>& Bytes) override
    {
        ++Writes; auto Callback=MoveTemp(DuringWrite); if(Callback) { Callback(); }
        if(bRejectWrites) { return false; }
        Banks.Add(FString::FromInt(User)+TEXT(":")+Slot,Bytes); return true;
    }
    virtual void PersistSettings() override { DeviceSavedUIScale=GetSettingsSnapshot().UIScale; }
};

UCLASS(Transient, NotBlueprintable)
class USovAccountInputRepairSettings : public UNarrativeInputSettings
{
    GENERATED_BODY()
public:
    TFunction<void()> DuringRegister;
    void Stage(ULocalPlayer* Player) { UEnhancedInputUserSettings::Initialize(Player); }
    void StageSensitivity(float Value) { AimSensitivity=Value; }
    virtual void SaveSettings() override {} // Never touch the engine's shared test-machine slot.
    virtual bool RegisterInputMappingContext(const UInputMappingContext* Context) override
    {
        const bool Registered=Super::RegisterInputMappingContext(Context);
        auto Callback=MoveTemp(DuringRegister); if(Callback) { Callback(); }
        return Registered;
    }
};
