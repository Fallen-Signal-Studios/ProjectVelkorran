// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerState.h"
#include "UI/SovHolographicHUDWidget.h"
#include "UI/SovHolographicHUDSurface.h"
#include "Tests/SovHolographicHUDTestFixtures.h"
#include "UI/SovHolographicHUDLayout.h"
#include "Sovereign/SovGameplayTags.h"
#include "NarrativeGameplayTags.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Components/EquipmentComponent.h"
#include "Tests/SovResourceTransactionRepairFixtures.h"

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
	static FSovHolographicHUDView View(const USovHolographicHUDWidget& Widget, const FVector2D& SafeSize)
	{ return Widget.BuildView(SafeSize); }
	static void SetSurface(USovHolographicHUDWidget& Widget, USovHolographicHUDSurface* Surface) { Widget.Surface = Surface; }
	static bool PaintDrew(const USovHolographicHUDWidget& Widget) { return Widget.bLastPaintDrew; }
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
	// Inject equipment state only; the production character lookup and snapshot decide what to publish.
	auto* Equipment = F.Player->FindComponentByClass<UEquipmentComponent>();
	if (!TestNotNull(TEXT("Player equipment exists"), Equipment)) { return false; }
	auto* Property = FindFProperty<FMapProperty>(UEquipmentComponent::StaticClass(), TEXT("WieldedWeapons"));
	if (!TestNotNull(TEXT("Wield map exists"), Property)) { return false; }
	auto* Wielded = Property->ContainerPtrToValuePtr<TMap<FGameplayTag, UWeaponItem*>>(Equipment);
	auto* Weapon = NewObject<USovResourceRepairWeapon>(F.Player);
	Wielded->Add(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand, Weapon);
	USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot);
	TestEqual(TEXT("An exhausted firearm still publishes its empty magazine"), Snapshot.AmmoInClip, 0);
	TestEqual(TEXT("An exhausted firearm still publishes its empty reserve"), Snapshot.AmmoReserve, 0);
	Weapon->ConfigureMagazine(nullptr, 1);
	USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot);
	TestEqual(TEXT("Wielded ammo-free weapon omits ammo"), Snapshot.AmmoInClip, -1);
	TestEqual(TEXT("Switching to melee clears the old reserve"), Snapshot.AmmoReserve, -1);
	Weapon->ConfigureMagazine(USovResourceRepairAmmo::StaticClass(), 0);
	USovHolographicHUDWidget::ReadSnapshot(F.Controller, Snapshot);
	TestEqual(TEXT("Ammo use without a magazine does not invent one"), Snapshot.AmmoInClip, -1);
	Wielded->Empty();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicViewTest,
	"ProjectVelkorran.Campaign.HolographicHUD.AuthoredSurfaceReceivesTheSameFrameThePainterWouldDraw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicViewTest::RunTest(const FString& Parameters)
{
	// The HUD keeps reading resources, grants, contacts and layout; only the drawing moves to an
	// authored widget. The frame it publishes must therefore agree with the layout every other
	// surface already avoids, and must not restate a resource the painter would have omitted.
	FHolographicWorld F;
	if (!TestTrue(TEXT("Ready holographic fixture"), F.Valid())) { return false; }
	// Constructed directly, as the sibling palette tests are: CreateWidget needs an attached local player.
	auto* Widget = NewObject<USovHolographicHUDWidget>(F.Player);
	if (!TestNotNull(TEXT("Holographic HUD widget"), Widget)) { return false; }

	FSovHolographicHUDSnapshot Snapshot;
	Snapshot.bValid = true;
	Snapshot.Vitals.Values[0] = {60.f, 120.f};   // Health
	Snapshot.Vitals.Values[1] = {0.f, 0.f};      // Shield, absent on this protagonist
	Snapshot.Echo = 35.f;
	Snapshot.MaxEcho = 100.f;
	Snapshot.AmmoInClip = -1;
	Snapshot.Settings.UIScale = 1.f;
	FSovProximityContact Contact;
	Contact.BearingDegrees = 90.f;               // Directly to the player's right.
	Contact.NormalisedRange = 1.f;
	Contact.Alpha = 1.f;
	Contact.bLiveSighting = true;
	Snapshot.Contacts.Add(Contact);
	FSovHolographicHUDTestAccess::SetSnapshot(*Widget, Snapshot);

	const FVector2D SafeSize(1600.f, 900.f);
	const FSovHolographicHUDView View = FSovHolographicHUDTestAccess::View(*Widget, SafeSize);
	TestTrue(TEXT("A live protagonist produces a usable frame"), View.bValid);
	TestEqual(TEXT("Health is divided once, for the surface"), View.Health.Fraction, .5f);
	// Dividing by a zero maximum would report an absent resource as completely full.
	TestEqual(TEXT("An absent shield reads as empty rather than full"), View.Shield.Fraction, 0.f);
	TestEqual(TEXT("Echo is divided from its own reading"), View.Echo.Fraction, .35f);
	TestFalse(TEXT("A magazine-less weapon publishes no ammo"), View.bHasAmmo);

	// The rectangles must be the ones the subtitle and caption surfaces are told to avoid, or an
	// authored HUD would sit somewhere the rest of the presentation does not expect it.
	const auto Layout = SovHolographicHUDLayout::Compute(SafeSize, 1.f);
	TestTrue(TEXT("The plate matches the shared layout"), View.Plate.Min.Equals(Layout.Plate.Min) && View.Plate.Max.Equals(Layout.Plate.Max));
	TestTrue(TEXT("The radar matches the shared layout"), View.Radar.Min.Equals(Layout.Radar.Min) && View.Radar.Max.Equals(Layout.Radar.Max));
	TestEqual(TEXT("The radar radius is the shared one"), View.RadarRadius, Layout.RadarRadius);

	// Screen up is the player's facing, so a contact due right sits on the +X axis of the disc.
	if (TestEqual(TEXT("The contact is published"), View.Contacts.Num(), 1))
	{
		TestTrue(TEXT("A contact to the right lands right of the radar centre"),
			View.Contacts[0].Offset.X > View.RadarRadius * .9f && FMath::Abs(View.Contacts[0].Offset.Y) < 1.f);
	}

	// With a surface installed the painter must stand down, or every readout would be drawn twice.
	auto* Surface = NewObject<USovHolographicHUDTestSurface>(F.Player);
	if (!TestNotNull(TEXT("Test surface"), Surface)) { return true; }
	Surface->ApplyHolographicHUDView(View);
	TestEqual(TEXT("A surface keeps the frame it was handed"), Surface->Applied().Health.Fraction, .5f);
	TestTrue(TEXT("The arc point walks the authored curve"),
		Surface->ArcPoint(0.f).Equals(View.ArcStart) && Surface->ArcPoint(1.f).Equals(View.ArcEnd));
	TestTrue(TEXT("A non-finite arc parameter falls back to the start rather than a NaN position"),
		Surface->ArcPoint(std::numeric_limits<float>::quiet_NaN()).Equals(View.ArcStart));
	return true;
}
#endif
