// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerState.h"
#include "UI/SovHolographicHUDWidget.h"
#include "Sovereign/SovGameplayTags.h"
#include "NarrativeGameplayTags.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
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
	static FLinearColor HealthFrom(const USovHolographicHUDWidget& Widget) { return Widget.BuildPalette().HealthFrom; }
	static FLinearColor HealthTo(const USovHolographicHUDWidget& Widget) { return Widget.BuildPalette().HealthTo; }
	static FLinearColor ShieldTo(const USovHolographicHUDWidget& Widget) { return Widget.BuildPalette().ShieldTo; }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicProtagonistPaletteTest,
	"ProjectVelkorran.Campaign.HolographicHUD.EachProtagonistGetsItsOwnPaletteUntilContrastOverridesIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicProtagonistPaletteTest::RunTest(const FString& Parameters)
{
	FHolographicWorld F;
	if (!TestTrue(TEXT("Ready holographic fixture"), F.Valid())) { return false; }
	// Constructed directly: a surface built through CreateWidget would need an attached local player.
	auto* Widget = NewObject<USovHolographicHUDWidget>(F.Player);
	if (!TestNotNull(TEXT("Surface constructs"), Widget)) { return false; }

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	FSovHolographicHUDSnapshot Snapshot;
	USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot);
	Snapshot.Settings.bHighContrastHUD = false;

	Snapshot.Vitals.Protagonist = Tags.Character_Player_Tarrik;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor TarrikHealth = FSovHolographicHUDTestAccess::HealthTo(*Widget);
	const FLinearColor TarrikAccent = FSovHolographicHUDTestAccess::Accent(*Widget);

	Snapshot.Vitals.Protagonist = Tags.Character_Player_Selene;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor SeleneHealth = FSovHolographicHUDTestAccess::HealthTo(*Widget);
	const FLinearColor SeleneAccent = FSovHolographicHUDTestAccess::Accent(*Widget);

	// A transparent bar is an absent readout, whichever protagonist is wearing it.
	TestEqual(TEXT("Tarrik's health bar is drawn opaque"), TarrikHealth.A, 1.f);
	TestEqual(TEXT("Selene's health bar is drawn opaque"), SeleneHealth.A, 1.f);
	// Both protagonists were asked for, so the identity branch has to actually resolve differently.
	TestFalse(TEXT("The protagonists do not share one accent"), TarrikAccent.Equals(SeleneAccent));
	TestFalse(TEXT("The protagonists do not share one health colour"), TarrikHealth.Equals(SeleneHealth));
	// Direction rather than exact values: the references read red for Tarrik and green for Selene.
	TestTrue(TEXT("Tarrik's health reads red"), TarrikHealth.R > TarrikHealth.G && TarrikHealth.R > TarrikHealth.B);
	TestTrue(TEXT("Selene's health reads green"), SeleneHealth.G > SeleneHealth.R);

	// Urgency was previously forced warm for both protagonists so that reading it never depended on
	// knowing whose HUD this is. Following the references' per-protagonist colours gives that up, and
	// what replaces it is the shield and cross glyphs plus this: contrast erases identity entirely.
	Snapshot.Settings.bHighContrastHUD = true;
	Snapshot.Vitals.Protagonist = Tags.Character_Player_Tarrik;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor TarrikContrast = FSovHolographicHUDTestAccess::HealthTo(*Widget);
	const FLinearColor ContrastShield = FSovHolographicHUDTestAccess::ShieldTo(*Widget);
	Snapshot.Vitals.Protagonist = Tags.Character_Player_Selene;
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);
	const FLinearColor SeleneContrast = FSovHolographicHUDTestAccess::HealthTo(*Widget);
	TestTrue(TEXT("High contrast resolves both protagonists to the same health colour"),
		TarrikContrast.Equals(SeleneContrast));
	// Thickened, never dropped: contrast must not cost a readout.
	TestEqual(TEXT("A high contrast health bar stays opaque"), TarrikContrast.A, 1.f);
	TestEqual(TEXT("A high contrast shield bar stays opaque"), ContrastShield.A, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicCinematicHideTest,
	"ProjectVelkorran.Campaign.HolographicHUD.ACinematicHideTagCollapsesTheSurfaceAndReleasingItRestoresIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicCinematicHideTest::RunTest(const FString& Parameters)
{
	FHolographicWorld F;
	if (!TestTrue(TEXT("Ready holographic fixture"), F.Valid())) { return false; }
	auto* ASC = F.Player->GetNarrativeAbilitySystemComponent();
	if (!TestNotNull(TEXT("The protagonist has an ability system"), ASC)) { return false; }

	FSovHolographicHUDSnapshot Snapshot;
	if (!TestTrue(TEXT("The surface reads a live protagonist to begin with"),
		USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot))) { return false; }

	// Narrative hides the HUD for a cinematic by adding this tag through a GameplayTag track; the
	// sequence actor's own bHideEvenEssentialHUDElements is deprecated in favour of it. The leaf is
	// applied here rather than the parent on purpose: the surface is protected only by parent
	// matching, so asserting the parent directly would exercise nothing that could actually break.
	const FGameplayTag Hide = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD_All;
	ASC->AddLooseGameplayTag(Hide);
	FSovHolographicHUDSnapshot Hidden;
	TestFalse(TEXT("A cinematic hide tag refuses the snapshot"),
		USovHolographicHUDWidget::ReadSnapshot(F.Controller, Hidden));
	TestFalse(TEXT("A refused snapshot is never left displayable"), Hidden.bValid);

	// Hiding has to be transient. A surface that never came back would be worse than one that never left.
	ASC->RemoveLooseGameplayTag(Hide);
	FSovHolographicHUDSnapshot Restored;
	TestTrue(TEXT("Releasing the cinematic restores the surface"),
		USovHolographicHUDWidget::ReadSnapshot(F.Controller, Restored));
	return true;
}
#endif
