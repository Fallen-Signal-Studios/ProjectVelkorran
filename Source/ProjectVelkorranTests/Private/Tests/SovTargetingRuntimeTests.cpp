// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Targeting/SovTargetingComponent.h"
#include "Targeting/SovAimAssist.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTargetingWorldTest, "ProjectVelkorran.Campaign.Targeting.NativeWorldAdmissionAndOcclusion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovTargetingWorldTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &WorldInitialization);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Player = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	auto* Enemy = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(600.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
	auto* Controller = World->SpawnActor<ANarrativePlayerController>();
	if (!Player || !Enemy || !Controller)
	{
		AddError(TEXT("Required actors failed to spawn")); World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } return false;
	}
	Player->InitializeTestCombat(0); Enemy->InitializeTestCombat(1);
	Controller->Possess(Player); Controller->SetViewTarget(Player); Controller->ResetIgnoreLookInput(); Controller->ResetIgnoreMoveInput();
	Player->GetNarrativeAbilitySystemComponent()->SetCharacterReadyEpoch(1);
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	USovTargetingComponent* Targeting = NewObject<USovTargetingComponent>(Player);
	Player->AddInstanceComponent(Targeting); Targeting->RegisterComponent(); Targeting->Controller = Controller;
	ESovLockLossReason Reason;
	TestFalse(TEXT("Actor must explicitly permit hard lock"), Targeting->IsValidTarget(Enemy, false, false, Reason));
	AActor* AimTarget = nullptr; FVector AimPoint;
	TestTrue(TEXT("Aim assistance can select a visible ordinary hostile without hard-lock permission"),
		SovAimAssist::FindVisibleTarget(Player, FVector::ZeroVector, FVector::ForwardVector, 1000.f, 8.f, AimTarget, AimPoint));
	TestTrue(TEXT("Aim assistance selects the actual nearby hostile"), AimTarget == Enemy);
	Enemy->Tags.Add(USovTargetingComponent::HardLockPermissionTag());
	TestTrue(TEXT("Living hostile permitted within range"), Targeting->IsValidTarget(Enemy, false, false, Reason));
	Enemy->TestTeam = 0;
	TestFalse(TEXT("Friendly rejected"), Targeting->IsValidTarget(Enemy, false, false, Reason));
	TestFalse(TEXT("Aim assistance never snaps to an ally"), SovAimAssist::FindVisibleTarget(Player,
		FVector::ZeroVector, FVector::ForwardVector, 1000.f, 8.f, AimTarget, AimPoint));
	Enemy->TestTeam = 1;
	Enemy->SetActorLocation(FVector(3000.f, 0.f, 0.f));
	TestFalse(TEXT("Range enforced"), Targeting->IsValidTarget(Enemy, false, false, Reason));
	TestEqual(TEXT("Range loss reason"), Reason, ESovLockLossReason::Distance);
	Enemy->SetActorLocation(FVector(600.f, 0.f, 0.f));
	TestFalse(TEXT("Absent nav relationship fails closed"), Targeting->IsValidTarget(Enemy, false, true, Reason));
	TestEqual(TEXT("Navigation loss reason"), Reason, ESovLockLossReason::Navigation);
	Targeting->bRequireNavigationRelationship = false;
	TestTrue(TEXT("Clear actual physics ray"), Targeting->HasLineOfSight(Enemy));
	AActor* Cover = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Cover); Cover->AddInstanceComponent(Box); Cover->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(20.f, 200.f, 200.f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Cover->SetActorLocation(FVector(300.f, 0.f, 0.f));
	TestFalse(TEXT("Opaque cover rejects actual ray"), Targeting->HasLineOfSight(Enemy));
	TestFalse(TEXT("Aim assistance cannot acquire a target through opaque cover"), SovAimAssist::FindVisibleTarget(Player,
		FVector::ZeroVector, FVector::ForwardVector, 1000.f, 8.f, AimTarget, AimPoint));
	Targeting->SetTarget(Enemy, ESovLockLossReason::None);
	const FVector OriginalLocation = Player->GetActorLocation();
	Targeting->TickComponent(.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Brief occlusion keeps lock"), Targeting->GetLockedTarget() == Enemy);
	Targeting->TickComponent(.3f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Occlusion timeout releases lock"), Targeting->GetLockedTarget());
	TestTrue(TEXT("Camera assistance never translates player"), Player->GetActorLocation().Equals(OriginalLocation));
	auto* LossProbe = NewObject<USovTargetingLossProbe>(Targeting);
	Targeting->OnLockTargetChanged.AddDynamic(LossProbe, &USovTargetingLossProbe::OnTargetChanged);
	Targeting->SetTarget(Enemy, ESovLockLossReason::None);
	Controller->UnPossess(); Targeting->TickComponent(.016f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Unpossessed owner loses its lock"), Targeting->GetLockedTarget());
	TestEqual(TEXT("Unpossess publishes one owner-unavailable loss"), LossProbe->LostCount, 1);
	TestEqual(TEXT("Loss reason identifies unavailable owner"), LossProbe->LastLoss, ESovLockLossReason::OwnerUnavailable);
	Targeting->TickComponent(.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("No repeated loss while unpossessed"), LossProbe->LostCount, 1);
	Enemy->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("Zero-health target rejected"), Targeting->IsValidTarget(Enemy, false, false, Reason));
	World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
	return true;
}
#endif
