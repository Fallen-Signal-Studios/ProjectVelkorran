// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovWeaponPairingTestFixtures.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "UI/SovAdsSightWidget.h"
#include "Weapons/SovAdsComponent.h"
#include "Weapons/SovAdsPolicy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS

/**
 * Drives the component's own state rather than fabricating a wielded weapon.
 *
 * Wielding runs through the equipment pipeline and a protected wield slot, which a content-free
 * test cannot honestly reach, so the aiming trigger itself stays covered by the portable policy
 * test and by play. What is covered here is what actually leaks in practice: a held field of view
 * that is never released, and a crosshair suppression that is never removed.
 */
struct FSovAdsRuntimeTestAccess
{
	static void Step(USovAdsComponent& Component, bool bAiming, float DeltaSeconds, ESovAdsSight Sight)
	{
		Component.ActiveSight = Sight;
		Component.AimBlend = SovAdsPolicy::AdvanceBlend(Component.AimBlend, bAiming, DeltaSeconds, Component.ProfileFor(Sight));
		auto* Owner = Cast<ANarrativeCharacter>(Component.GetOwner());
		auto* Controller = Owner ? Cast<ASovPlayerController>(Owner->GetController()) : nullptr;
		if (Component.AimBlend > 0.f) { Component.ApplyFieldOfView(Controller, Component.ProfileFor(Sight)); }
		else { Component.ReleaseFieldOfView(Controller); }
		Component.RefreshCrosshairSuppression(Owner, SovAdsPolicy::ReplacesCrosshair(Component.AimBlend, Component.ProfileFor(Sight)));
	}
	static ESovAdsSight SightFor(const USovAdsComponent& Component, const UWeaponItem* Weapon)
	{ return Component.SightForWeapon(Weapon); }
	static bool IsSuppressingCrosshair(const USovAdsComponent& Component) { return Component.bSuppressingCrosshair; }
};

namespace
{
struct FAdsWorld
{
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = nullptr;
	ASovMeleeRuntimeTestPlayer* Player = nullptr;
	ASovHandoffRuntimeTestController* Controller = nullptr;
	UNarrativeAbilitySystemComponent* ASC = nullptr;
	USovAdsComponent* Ads = nullptr;

	FAdsWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		World->InitializeActorsForPlay(FURL());
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ASovMeleeRuntimeTestPlayer>(ASovMeleeRuntimeTestPlayer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
		auto* PlayerState = World->SpawnActor<ASovPlayerState>();
		if (!Player || !Controller || !PlayerState) { return; }
		auto* Definition = NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
		Player->PrepareCampaignInitialization(Definition); Controller->SetTestPlayerState(PlayerState);
		// Deliberately not marked as a local player controller. Narrative creates its gameplay HUD on
		// BeginPlay guarded only by IsLocalPlayerController, with no check for an attached player, so
		// claiming local ownership here makes it build a widget against a controller that has none.
		// Nothing under test needs it: the camera manager is spawned explicitly below.
		World->AddController(Controller); Controller->Possess(Player);
		if (!Player->StageTestReadiness(PlayerState, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
		if (!Controller->PlayerCameraManager) { Controller->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(); }
		if (!Controller->PlayerCameraManager) { return; }
		Controller->PlayerCameraManager->InitializeFor(Controller);
		Controller->SetViewTarget(Player);
		ASC = Player->GetNarrativeAbilitySystemComponent();
		Ads = NewObject<USovAdsComponent>(Player);
		Player->AddInstanceComponent(Ads); Ads->RegisterComponent();
		// This world becomes the current world context, so the editor loop would tick the component
		// and drive its sight widget against a controller that has no local player. These tests are
		// about releasing the camera and restoring the crosshair, so only explicit steps advance it.
		Ads->SetComponentTickEnabled(false);
	}
	~FAdsWorld()
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	bool Valid() const { return World && Player && Controller && ASC && Ads; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAdsZoomReleaseTest, "ProjectVelkorran.Campaign.Ads.ZoomIsHeldWhileAimingAndReleasedAfter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAdsZoomReleaseTest::RunTest(const FString& Parameters)
{
	FAdsWorld F;
	if (!TestTrue(TEXT("Ready aiming fixture"), F.Valid())) { return false; }
	TestEqual(TEXT("Nothing is held at the hip"), F.Ads->GetHeldFieldOfView(), 0.f);

	// Raise the sight over several frames, as a player holding aim would.
	for (int32 Frame = 0; Frame < 30; ++Frame) { FSovAdsRuntimeTestAccess::Step(*F.Ads, true, .016f, ESovAdsSight::Scope); }
	TestEqual(TEXT("A raised sight reaches full blend"), F.Ads->GetAimBlend(), 1.f);
	if (!TestTrue(TEXT("Aiming holds a field of view"), F.Ads->GetHeldFieldOfView() > 0.f)) { return false; }
	TestTrue(TEXT("The held field of view is narrower than the unaimed view"),
		F.Ads->GetHeldFieldOfView() < F.Controller->PlayerCameraManager->DefaultFOV);

	// Lowering it must give the camera back. A zoom that is never released is the worst failure here.
	for (int32 Frame = 0; Frame < 40; ++Frame) { FSovAdsRuntimeTestAccess::Step(*F.Ads, false, .016f, ESovAdsSight::Scope); }
	TestEqual(TEXT("A lowered sight returns to the hip"), F.Ads->GetAimBlend(), 0.f);
	TestEqual(TEXT("Lowering releases the camera rather than leaving it locked"), F.Ads->GetHeldFieldOfView(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAdsCrosshairTest, "ProjectVelkorran.Campaign.Ads.SightPictureReplacesAndRestoresTheCrosshair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAdsCrosshairTest::RunTest(const FString& Parameters)
{
	FAdsWorld F;
	if (!TestTrue(TEXT("Ready aiming fixture"), F.Valid())) { return false; }
	const FGameplayTag Hide = FNarrativeGameplayTags::Get().State_UI_HideCrosshair;
	TestFalse(TEXT("The hip crosshair is shown at rest"), F.ASC->HasMatchingGameplayTag(Hide));

	for (int32 Frame = 0; Frame < 30; ++Frame) { FSovAdsRuntimeTestAccess::Step(*F.Ads, true, .016f, ESovAdsSight::Scope); }
	TestTrue(TEXT("A real sight picture replaces the hip crosshair"), F.ASC->HasMatchingGameplayTag(Hide));
	TestTrue(TEXT("The component knows it owns that suppression"), FSovAdsRuntimeTestAccess::IsSuppressingCrosshair(*F.Ads));

	for (int32 Frame = 0; Frame < 40; ++Frame) { FSovAdsRuntimeTestAccess::Step(*F.Ads, false, .016f, ESovAdsSight::Scope); }
	TestFalse(TEXT("Lowering the sight restores the crosshair rather than leaving it hidden"),
		F.ASC->HasMatchingGameplayTag(Hide));
	TestFalse(TEXT("No suppression is left owned"), FSovAdsRuntimeTestAccess::IsSuppressingCrosshair(*F.Ads));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAdsSightSelectionTest, "ProjectVelkorran.Campaign.Ads.UnmatchedWeaponsFallBackToADefaultSight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAdsSightSelectionTest::RunTest(const FString& Parameters)
{
	FAdsWorld F;
	if (!TestTrue(TEXT("Ready aiming fixture"), F.Valid())) { return false; }
	// A ranged weapon that is not one of the authored entries still gets a readable sight rather
	// than nothing. This is also what happens on a machine where an untracked weapon cannot resolve.
	TStrongObjectPtr<USovWeaponPairingTestRanged> Unlisted(NewObject<USovWeaponPairingTestRanged>(F.Player));
	TestEqual(TEXT("An unlisted weapon uses the default sight"),
		FSovAdsRuntimeTestAccess::SightFor(*F.Ads, Unlisted.Get()), ESovAdsSight::Default);
	TestEqual(TEXT("No weapon at all is never treated as a scope"),
		FSovAdsRuntimeTestAccess::SightFor(*F.Ads, nullptr), ESovAdsSight::Default);

	// The default sight still frames the shot, and iron sights never black out the screen.
	TestTrue(TEXT("Iron sights never take the whole screen"),
		SovAdsPolicy::SightAlpha(1.f, SovAdsPolicy::Cinderline) < 1.f);
	TestEqual(TEXT("A scope does take the screen once settled"),
		SovAdsPolicy::SightAlpha(1.f, SovAdsPolicy::Staccato), 1.f);
	return true;
}
#endif
