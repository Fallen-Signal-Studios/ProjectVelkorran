// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovStatusCheckpointTestFixtures.h"
#include "Companions/SovCompanionApproachPolicy.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/SovCombatTypes.h"
#include "NarrativeGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovDesignationTests
{
	struct FWorldScope
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		uint64 FixtureFrame = GFrameCounter;
		FWorldScope()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
				World->GetTimerManager().Tick(0.f);
			}
		}
		~FWorldScope() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
		ASovStatusCheckpointTestActor* Threat() const
		{
			auto* Result = World ? World->SpawnActor<ASovStatusCheckpointTestActor>() : nullptr;
			if (Result) { Result->InitializeCombat(); }
			return Result;
		}
		/** Expiry runs on the status component's own timer, which only ticks once per frame. */
		void AdvanceTimers(float Seconds)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FixtureFrame);
			World->GetTimerManager().Tick(Seconds);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDesignationWindowTest,
	"ProjectVelkorran.Campaign.Targeting.DesignationWindowsAreRealStatuses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovDesignationWindowTest::RunTest(const FString& Parameters)
{
	// Selene's priority mark and Tarrik's command target are read from the target's own tags at the
	// killing hit, and the Resonance SupportSever offer reads the mark. Nothing produced either state
	// (audit PC2-06), so both are now built-in status definitions with a real window and a cleanse.
	using namespace SovDesignationTests;
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	FWorldScope Scope;
	auto* Threat = Scope.Threat();
	auto* Source = Scope.World ? Scope.World->SpawnActor<AActor>() : nullptr;
	if (!TestNotNull(TEXT("Threat fixture"), Threat) || !TestNotNull(TEXT("Designating protagonist"), Source)) { return false; }
	auto* Status = Threat->Status.Get();
	auto* ASC = Threat->ASC.Get();
	if (!TestNotNull(TEXT("Threat status owner"), Status) || !TestNotNull(TEXT("Threat ability system"), ASC)) { return false; }

	for (const TPair<FGameplayTag, FGameplayTag> Designation :
		{ TPair<FGameplayTag, FGameplayTag>(Tags.Status_Apply_Mark, Tags.State_Target_Marked),
		  TPair<FGameplayTag, FGameplayTag>(Tags.Status_Apply_CommandTarget, Tags.State_CommandTarget_Window) })
	{
		const FString Named = Designation.Key.ToString();
		TestEqual(*(Named + TEXT(" is a defined status")), Status->ApplyStatusByTag(Designation.Key, Source),
			ESovStatusApplicationResult::Applied);
		TestTrue(*(Named + TEXT(" publishes its target-owned state")), ASC->HasMatchingGameplayTag(Designation.Value));
		TestTrue(*(Named + TEXT(" records the protagonist who designated")), Status->WasStatusAppliedBy(Designation.Key, Source));
		TestTrue(*(Named + TEXT(" opens a finite window")), Status->GetStatusRemainingDuration(Designation.Key) > 0.f);
		// A designation is a reward window, not a debuff: it must not constrain the target it names.
		TestFalse(*(Named + TEXT(" never busies its target")), ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
		TestFalse(*(Named + TEXT(" never locks its target's movement")),
			ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Lock));
	}
	TestTrue(TEXT("Both designations can be held at once"),
		Status->HasActiveStatus(Tags.Status_Apply_Mark) && Status->HasActiveStatus(Tags.Status_Apply_CommandTarget));
	// Re-designating the same threat refreshes the window rather than stacking a second one.
	TestEqual(TEXT("Designating again refreshes the window"), Status->ApplyStatusByTag(Tags.Status_Apply_Mark, Source),
		ESovStatusApplicationResult::Refreshed);
	TestEqual(TEXT("A designation never stacks"), Status->GetStatusStackCount(Tags.Status_Apply_Mark), 1);

	TestEqual(TEXT("One cleanse retires every designation"), Status->CleanseStatuses(Tags.Status_Cleanse_Designation), 2);
	TestFalse(TEXT("The cleansed mark leaves no state behind"), ASC->HasMatchingGameplayTag(Tags.State_Target_Marked));
	TestFalse(TEXT("The cleansed command target leaves no state behind"), ASC->HasMatchingGameplayTag(Tags.State_CommandTarget_Window));

	// The window closes on its own. Nine seconds outruns both eight-second designations.
	Status->ApplyStatusByTag(Tags.Status_Apply_Mark, Source);
	TestTrue(TEXT("A fresh mark is live"), ASC->HasMatchingGameplayTag(Tags.State_Target_Marked));
	Scope.AdvanceTimers(5.f);
	Scope.AdvanceTimers(5.f);
	TestFalse(TEXT("An unspent mark expires"), ASC->HasMatchingGameplayTag(Tags.State_Target_Marked));
	TestFalse(TEXT("Expiry retires the status record with its state"), Status->HasActiveStatus(Tags.Status_Apply_Mark));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionApproachPolicyTest,
	"ProjectVelkorran.Campaign.Companion.OrderedFocusIsApproached", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCompanionApproachPolicyTest::RunTest(const FString& Parameters)
{
	// A commanded companion refused to walk to a focus more than ten metres from the leader, and
	// refused entirely without line of sight, so ordering it onto a target behind a crate fifteen
	// metres ahead did nothing (audit EA2-07). The order's own reach is the leash now.
	using namespace SovCompanionApproachPolicy;
	const FVector Leader(0.f, 0.f, 0.f);
	const FVector Companion(-200.f, 0.f, 0.f);
	FVector Point;

	const FVector Ordered(1500.f, 0.f, 0.f);
	TestTrue(TEXT("An ordered focus fifteen metres out is approached"),
		SelectApproachPoint(Companion, Ordered, Leader, 150.f, 250.f, 400.f, OrderedLeash, Point));
	TestTrue(TEXT("The companion stops at its own attack range, not on top of the threat"),
		FVector::Dist(Point, Ordered) >= 150.f - KINDA_SMALL_NUMBER && FVector::Dist(Point, Ordered) <= 400.f);
	TestTrue(TEXT("The approach point is on the companion's side of the threat"), Point.X < Ordered.X);

	TestFalse(TEXT("The same focus is out of reach for a focus the companion picked up itself"),
		SelectApproachPoint(Companion, Ordered, Leader, 150.f, 250.f, 400.f, UnorderedLeash, Point));

	TestFalse(TEXT("An ordered focus beyond the order's own reach is refused"),
		SelectApproachPoint(Companion, FVector(4000.f, 0.f, 0.f), Leader, 150.f, 250.f, 400.f, OrderedLeash, Point));
	TestTrue(TEXT("A refusal never leaves a stale destination"), Point.IsZero());

	TestFalse(TEXT("A threat already within attack range needs no approach"),
		SelectApproachPoint(FVector(1200.f, 0.f, 0.f), Ordered, Leader, 150.f, 250.f, 400.f, OrderedLeash, Point));

	// Degenerate inputs must fail closed rather than steer a companion to an unreal place.
	TestFalse(TEXT("Non-finite range is refused"),
		SelectApproachPoint(Companion, Ordered, Leader, 150.f, 250.f, std::numeric_limits<float>::quiet_NaN(), OrderedLeash, Point));
	TestFalse(TEXT("A NaN focus is refused"),
		SelectApproachPoint(Companion, FVector(NAN, 0.f, 0.f), Leader, 150.f, 250.f, 400.f, OrderedLeash, Point));
	TestTrue(TEXT("A focus directly above the companion still yields a horizontal stand-off"),
		SelectApproachPoint(FVector(0.f, 0.f, 500.f), FVector::ZeroVector, Leader, 150.f, 250.f, 400.f, OrderedLeash, Point)
		&& FMath::IsNearlyZero(Point.Z));
	TestTrue(TEXT("Preferred range is respected between the minimum and maximum"),
		FMath::IsNearlyEqual(ApproachRange(150.f, 250.f, 400.f), 250.f));
	TestTrue(TEXT("A preferred range past the safe fraction of maximum is clamped"),
		ApproachRange(150.f, 9000.f, 400.f) <= FMath::Lerp(150.f, 400.f, .75f) + KINDA_SMALL_NUMBER);
	return true;
}
#endif
