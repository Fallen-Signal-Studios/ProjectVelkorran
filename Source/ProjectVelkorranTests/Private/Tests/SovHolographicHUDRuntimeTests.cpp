// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerState.h"
#include "UI/SovHolographicHUDWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS

/** Reaches the palette without a viewport: the surface is constructed directly, never through CreateWidget. */
struct FSovHolographicHUDTestAccess
{
	static void SetSnapshot(USovHolographicHUDWidget& Widget, const FSovHolographicHUDSnapshot& Snapshot)
	{ Widget.Displayed = Snapshot; }
	static FLinearColor Backing(const USovHolographicHUDWidget& Widget) { return Widget.BuildPalette().Backing; }
	static FLinearColor Accent(const USovHolographicHUDWidget& Widget) { return Widget.BuildPalette().Accent; }
};

namespace
{
struct FHolographicWorld
{
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = nullptr;
	ASovMeleeRuntimeTestPlayer* Player = nullptr;
	ASovHandoffRuntimeTestController* Controller = nullptr;

	FHolographicWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ASovMeleeRuntimeTestPlayer>(ASovMeleeRuntimeTestPlayer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
		auto* PlayerState = World->SpawnActor<ASovPlayerState>();
		if (!Player || !Controller || !PlayerState) { return; }
		auto* Definition = NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
		Player->PrepareCampaignInitialization(Definition); Controller->SetTestPlayerState(PlayerState);
		World->AddController(Controller); Controller->Possess(Player);
		Player->StageTestReadiness(PlayerState, true); Player->CompleteCampaignDataInitialization(false);
	}
	~FHolographicWorld()
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	bool Valid() const { return World && Player && Controller; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicSnapshotTest, "ProjectVelkorran.Campaign.HolographicHUD.AmmoIsOmittedRatherThanShownAsZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicSnapshotTest::RunTest(const FString& Parameters)
{
	FHolographicWorld F;
	if (!TestTrue(TEXT("Ready holographic fixture"), F.Valid())) { return false; }
	FSovHolographicHUDSnapshot Snapshot;
	if (!TestTrue(TEXT("The surface reads a live protagonist"), USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot)))
	{ return false; }
	TestTrue(TEXT("The snapshot is usable"), Snapshot.bValid);
	// An unarmed protagonist has no magazine to report. Showing "0 / 0" would state something false.
	TestEqual(TEXT("No wielded weapon omits the ammo readout"), Snapshot.AmmoInClip, -1);
	TestEqual(TEXT("No wielded weapon omits the reserve too"), Snapshot.AmmoReserve, -1);
	// Detection is present on every protagonist, and reports nothing with no hostiles in the world.
	TestEqual(TEXT("An empty world produces no radar contacts"), Snapshot.Contacts.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicContrastTest, "ProjectVelkorran.Campaign.HolographicHUD.HighContrastThickensBackingRatherThanDroppingInformation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicContrastTest::RunTest(const FString& Parameters)
{
	FHolographicWorld F;
	if (!TestTrue(TEXT("Ready holographic fixture"), F.Valid())) { return false; }
	// Constructed directly: a surface built through CreateWidget would need an attached local player.
	auto* Widget = NewObject<USovHolographicHUDWidget>(F.Player);
	if (!TestNotNull(TEXT("Surface constructs"), Widget)) { return false; }

	FSovHolographicHUDSnapshot Snapshot;
	USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot);
	Snapshot.Settings.bHighContrastHUD = false;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor Veil = FSovHolographicHUDTestAccess::Backing(*Widget);
	TestTrue(TEXT("The ordinary backing is a translucent optical veil"), Veil.A > 0.f && Veil.A < 1.f);

	Snapshot.Settings.bHighContrastHUD = true;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor Opaque = FSovHolographicHUDTestAccess::Backing(*Widget);
	TestEqual(TEXT("High contrast makes the backing fully opaque"), Opaque.A, 1.f);
	// Contrast is achieved by thickening the backing, never by removing a readout.
	TestTrue(TEXT("High contrast keeps a visible accent for the readout"),
		FSovHolographicHUDTestAccess::Accent(*Widget).A > 0.f);
	return true;
}
#endif
