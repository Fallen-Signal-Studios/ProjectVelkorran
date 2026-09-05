// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAudioSettingsTestFixtures.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovIndependentAudioBusRuntime, "ProjectVelkorran.Campaign.Audio.IndependentAmbienceAndTinnitusBuses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovIndependentAudioBusRuntime::RunTest(const FString&)
{
	TStrongObjectPtr<USovAudioSettingsProbe> Settings(NewObject<USovAudioSettingsProbe>()); Settings->InitializeAudio();
	Settings->SetOverallAudioVolume(.7f); Settings->SetSFXAudioVolume(.6f);
	Settings->SetAmbienceAudioVolume(.25f); Settings->SetTinnitusAudioVolume(0.f);
	TestEqual(TEXT("All seven actual configured SoundClass objects receive independent overrides"), Settings->SubmittedVolumes.Num(), 7);
	TestEqual(TEXT("Master volume remains independent"), Settings->SubmittedVolumes.FindRef(Settings->Classes[0]), .7f);
	TestEqual(TEXT("Effects remain independent"), Settings->SubmittedVolumes.FindRef(Settings->Classes[1]), .6f);
	TestEqual(TEXT("Ambience slider reaches the real existing settings-to-bus path"), Settings->SubmittedVolumes.FindRef(Settings->Classes[5]), .25f);
	TestEqual(TEXT("Tinnitus mute is an exact zero bus override"), Settings->SubmittedVolumes.FindRef(Settings->Classes[6]), 0.f);
	TestTrue(TEXT("Each explicit slider action persists through existing settings hook"), Settings->SaveCount >= 4);
	Settings->Configuration->TinnitusSoundClass = Settings->Configuration->MasterSoundClass;
	Settings->SubmittedVolumes.Reset(); Settings->ApplySoundSettings();
	TestEqual(TEXT("Aliased tinnitus bus cannot overwrite Master"), Settings->SubmittedVolumes.FindRef(Settings->Classes[0]), .7f);
	TestEqual(TEXT("Aliased bus is rejected, not applied twice"), Settings->SubmittedVolumes.Num(), 6);
	Settings->InjectConfig(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 255);
	Settings->ApplySoundSettings();
	TestEqual(TEXT("NaN configuration is normalized before output"), Settings->GetAmbienceAudioVolume(), 1.f);
	TestEqual(TEXT("Infinite configuration is normalized before output"), Settings->GetTinnitusAudioVolume(), 1.f);
	TestEqual(TEXT("Unknown persisted range normalizes to Full"), Settings->GetAudioDynamicRange(), ENarrativeAudioDynamicRange::Full);
	Settings->SetAmbienceAudioVolume(-10.f); Settings->SetTinnitusAudioVolume(9.f);
	TestEqual(TEXT("Volume lower bound"), Settings->GetAmbienceAudioVolume(), 0.f);
	TestEqual(TEXT("Volume upper bound"), Settings->GetTinnitusAudioVolume(), 1.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAuthoredDynamicRangeRuntime, "ProjectVelkorran.Campaign.Audio.AuthoredRangeSelectionAndFallback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovAuthoredDynamicRangeRuntime::RunTest(const FString&)
{
	TStrongObjectPtr<USovAudioSettingsProbe> Settings(NewObject<USovAudioSettingsProbe>()); Settings->InitializeAudio();
	TestTrue(TEXT("Configured persistent authored Reduced mix is available"), Settings->IsAudioDynamicRangeAvailable(ENarrativeAudioDynamicRange::Reduced));
	Settings->SetAudioDynamicRange(ENarrativeAudioDynamicRange::Reduced);
	TestTrue(TEXT("Exact authored Reduced SoundMix reaches device seam"), Settings->SubmittedRange == Settings->ReducedMix);
	TestEqual(TEXT("Applied range is Reduced"), Settings->GetAppliedAudioDynamicRange(), ENarrativeAudioDynamicRange::Reduced);
	Settings->SetAudioDynamicRange(ENarrativeAudioDynamicRange::Night);
	TestTrue(TEXT("Changing range replaces authored mix selection"), Settings->SubmittedRange == Settings->NightMix);
	Settings->NightMix->Duration = 3.f;
	TestFalse(TEXT("Timed sound effect cannot represent persistent range preference"), Settings->IsAudioDynamicRangeAvailable(ENarrativeAudioDynamicRange::Night));
	Settings->ApplySoundSettings();
	TestFalse(TEXT("Invalid range asset requests removal of owned modifier"), Settings->SubmittedRange != nullptr);
	TestEqual(TEXT("Unavailable authored preset visibly falls back to Full"), Settings->GetAppliedAudioDynamicRange(), ENarrativeAudioDynamicRange::Full);
	Settings->NightMix->Duration = -1.f; Settings->Configuration->NightDynamicRangeSoundMix.Reset();
	TestFalse(TEXT("Missing optional preset is unavailable"), Settings->IsAudioDynamicRangeAvailable(ENarrativeAudioDynamicRange::Night));
	Settings->Configuration->ReducedDynamicRangeSoundMix = FSoftObjectPath(Settings->BaseMix);
	TestFalse(TEXT("Base volume mix cannot be pushed as an additional range modifier"), Settings->IsAudioDynamicRangeAvailable(ENarrativeAudioDynamicRange::Reduced));
	Settings->Configuration->ReducedDynamicRangeSoundMix = FSoftObjectPath(Settings->ReducedMix);
	Settings->SetAudioDynamicRange(ENarrativeAudioDynamicRange::Reduced);
	const int32 BeforeLoss = Settings->BusCalls; Settings->bOutputAvailable = false; Settings->ApplySoundSettings();
	TestEqual(TEXT("Device absence does not drive bus output"), Settings->BusCalls, BeforeLoss);
	TestFalse(TEXT("Device absence releases range selection"), Settings->SubmittedRange != nullptr);
	Settings->bOutputAvailable = true; Settings->ApplySoundSettings();
	TestTrue(TEXT("Restored device reapplies saved authored preference"), Settings->SubmittedRange == Settings->ReducedMix);
	return true;
}
#endif
