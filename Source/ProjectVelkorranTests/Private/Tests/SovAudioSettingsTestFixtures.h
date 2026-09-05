// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "ArsenalSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "SovAudioSettingsTestFixtures.generated.h"

UCLASS()
class USovAudioSettingsProbe : public UNarrativeGameUserSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) TObjectPtr<UArsenalSettings> Configuration;
	UPROPERTY(Transient) TObjectPtr<USoundMix> BaseMix;
	UPROPERTY(Transient) TObjectPtr<USoundMix> ReducedMix;
	UPROPERTY(Transient) TObjectPtr<USoundMix> NightMix;
	UPROPERTY(Transient) TObjectPtr<USoundMix> SubmittedRange;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> Classes;
	TMap<USoundClass*, float> SubmittedVolumes;
	bool bOutputAvailable = true;
	int32 SaveCount = 0;
	int32 BusCalls = 0;
	void InitializeAudio()
	{
		Configuration = NewObject<UArsenalSettings>(this);
		BaseMix = NewObject<USoundMix>(this); ReducedMix = NewObject<USoundMix>(this); NightMix = NewObject<USoundMix>(this);
		ReducedMix->Duration = -1.f; NightMix->Duration = -1.f;
		FSoftObjectPath* Paths[] = { &Configuration->MasterSoundClass, &Configuration->SFXSoundClass,
			&Configuration->UISoundClass, &Configuration->DialogueSoundClass, &Configuration->MusicSoundClass,
			&Configuration->AmbienceSoundClass, &Configuration->TinnitusSoundClass };
		for (auto* Path : Paths) { USoundClass* Class = NewObject<USoundClass>(this); Classes.Add(Class); *Path = FSoftObjectPath(Class); }
		Configuration->ReducedDynamicRangeSoundMix = FSoftObjectPath(ReducedMix);
		Configuration->NightDynamicRangeSoundMix = FSoftObjectPath(NightMix);
	}
	void InjectConfig(float Ambience, float Tinnitus, unsigned Range)
	{ AmbienceAudioVolume = Ambience; TinnitusAudioVolume = Tinnitus; AudioDynamicRange = static_cast<ENarrativeAudioDynamicRange>(Range); }
	virtual void SaveSettings() override { ++SaveCount; }
protected:
	virtual bool HasAudioOutput() const override { return bOutputAvailable; }
	virtual const UArsenalSettings* ReadAudioConfiguration() const override { return Configuration; }
	virtual USoundMix* ReadAudioBaseMix() const override { return BaseMix; }
	virtual void SubmitSoundClassVolume(USoundMix* Mix, USoundClass* Class, float Volume) override
	{ if (Mix == BaseMix && Class) { ++BusCalls; SubmittedVolumes.Add(Class, Volume); } }
	virtual void SubmitDynamicRangeMix(USoundMix* Mix) override { SubmittedRange = Mix; }
};
