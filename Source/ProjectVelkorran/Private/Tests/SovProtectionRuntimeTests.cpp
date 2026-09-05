// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtectionRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Combat/SovProtectionInterceptReceipt.h"
#include "AIController.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FProtectionWorld
	{
		UWorld* World = nullptr;
		ASovAxiomRuntimeTestCharacter* Threat = nullptr;
		ASovAxiomRuntimeTestCharacter* Tarrik = nullptr;
		ASovAxiomRuntimeTestCharacter* Ally = nullptr;
		USovTarrikEchoGenerationComponent* Generator = nullptr;
		AAIController* Controller = nullptr;
		FProtectionWorld()
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &WorldInitialization);
			if (!World) return;
			if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Threat = Character(0.f, 1);
			Tarrik = Character(400.f, 0);
			Ally = Character(800.f, 0);
			if (!Threat || !Tarrik || !Ally) return;
			auto* ASC = Tarrik->GetNarrativeAbilitySystemComponent();
			ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
			ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
			Tarrik->SetActorRotation(FRotator(0.f, 180.f, 0.f));
			Generator = NewObject<USovTarrikEchoGenerationComponent>(Tarrik);
			Tarrik->AddInstanceComponent(Generator); Generator->RegisterComponent();
			Generator->InitializeWithAbilitySystem(ASC);
			Tarrik->TestEcho->RestoreEchoFromCheckpoint(0.f);
			Controller = Focus(Threat);
		}
		~FProtectionWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); }
		}
		bool Ready() const { return World && Threat && Tarrik && Ally && Generator && Generator->IsInitialized() && Controller; }
		ASovAxiomRuntimeTestCharacter* Character(float X, int32 Team)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* C = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),
				FVector(X, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
			if (C)
			{
				C->InitializeTestCombat(Team);
				C->GetCapsuleComponent()->SetCollisionResponseToChannel(Channel(), ECR_Block);
			}
			return C;
		}
		AAIController* Focus(ASovAxiomRuntimeTestCharacter* C)
		{
			auto* AI = World->SpawnActor<AAIController>();
			if (AI) { AI->Possess(C); AI->SetFocus(Ally); }
			return AI;
		}
		static ECollisionChannel Channel() { return UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel; }
		FHitResult Trace(float Radius = 0.f) const
		{
			FCollisionQueryParams Query = Threat->GetIgnoreCharacterParams();
			Query.bTraceComplex = true; Query.bReturnPhysicalMaterial = true;
			FHitResult Hit;
			if (FMath::IsNearlyZero(Radius)) World->LineTraceSingleByChannel(Hit, FVector(100.f, 0.f, 0.f), FVector(2100.f, 0.f, 0.f), Channel(), Query);
			else World->SweepSingleByChannel(Hit, FVector(100.f, 0.f, 0.f), FVector(2100.f, 0.f, 0.f), FQuat::Identity,
				Channel(), FCollisionShape::MakeSphere(Radius), Query);
			return Hit;
		}
		USovProtectionInterceptReceipt* Proof(float Radius = 0.f) const
		{
			return USovProtectionInterceptReceipt::TryCreateForDroneShot(Threat, Ally, Trace(Radius),
				FVector(100.f, 0.f, 0.f), FVector(2100.f, 0.f, 0.f), Radius);
		}
		AActor* Cover(float X, AActor* Owner = nullptr)
		{
			auto* Cover = World->SpawnActor<AActor>();
			if (!Cover) return nullptr;
			auto* Box = NewObject<UBoxComponent>(Cover);
			Cover->AddInstanceComponent(Box); Cover->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(20.f, 60.f, 60.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(Channel(), ECR_Block);
			Box->RegisterComponent(); Cover->SetActorLocation(FVector(X, 0.f, 0.f)); Cover->SetOwner(Owner);
			return Cover;
		}
		USovProtectionRuntimeGunfire* Activate(ASovAxiomRuntimeTestCharacter* Source = nullptr)
		{
			if (!Source) Source = Threat;
			auto* ASC = Source->GetNarrativeAbilitySystemComponent();
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(
				USovProtectionRuntimeGunfire::StaticClass(), 1, INDEX_NONE, Source->SetTestWeapon()));
			if (!ASC->TryActivateAbility(Handle)) return nullptr;
			auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec ? Cast<USovProtectionRuntimeGunfire>(Spec->GetPrimaryInstance()) : nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtectionGeometryRuntimeTest,
	"ProjectVelkorran.Campaign.Echo.Protection.Geometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtectionGeometryRuntimeTest::RunTest(const FString&)
{
	FProtectionWorld F;
	if (!TestTrue(TEXT("Real character/ASC/AI world"), F.Ready())) return false;
	TestEqual(TEXT("Original shot first hits Tarrik"), F.Trace().GetActor(), static_cast<AActor*>(F.Tarrik));
	TestNotNull(TEXT("Identical counterfactual ray reaches focused ally"), F.Proof());
	TestNotNull(TEXT("Sphere sweep keeps the same counterfactual geometry"), F.Proof(2.f));
	F.Controller->ClearFocus(EAIFocusPriority::Gameplay);
	TestNull(TEXT("Taking damage without an intended AI focus is not protection"), F.Proof());
	F.Controller->SetFocus(F.Tarrik);
	TestNull(TEXT("A shot intentionally aimed at Tarrik gives no protection reward"), F.Proof());
	F.Controller->SetFocus(F.Ally);
	F.Ally->TestTeam = 1;
	TestNull(TEXT("Ally must be friendly to Tarrik and hostile to shooter"), F.Proof());
	F.Ally->TestTeam = 0;
	F.Threat->TestTeam = 0;
	TestNull(TEXT("Friendly shooter cannot generate protection credit"), F.Proof());
	F.Threat->TestTeam = 1;
	F.Ally->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestNull(TEXT("Dead intended ally cannot mint a receipt"), F.Proof());
	F.Ally->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	AActor* Wall = F.Cover(600.f);
	if (!TestNotNull(TEXT("World cover"), Wall)) return false;
	TestNull(TEXT("Wall between protector and ally remains a blocker"), F.Proof());
	Wall->SetOwner(F.Ally);
	TestNull(TEXT("Ally ownership cannot turn cover into the intended victim"), F.Proof());
	Wall->SetOwner(F.Tarrik);
	Wall->AttachToActor(F.Tarrik, FAttachmentTransformRules::KeepWorldTransform);
	TestNull(TEXT("Arbitrary protector-owned attached cover is never ignored"), F.Proof());
	Wall->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Wall->SetActorLocation(FVector(200.f, 0.f, 0.f));
	TestNull(TEXT("Wall before Tarrik is not an intercepted body hit"), F.Proof());
	Wall->SetActorEnableCollision(false);
	auto* Alternate = F.Character(600.f, 0);
	if (!TestNotNull(TEXT("Alternate ally"), Alternate)) return false;
	TestNull(TEXT("First counterfactual victim must be the exact intended ally"), F.Proof());
	Alternate->SetActorEnableCollision(false);
	TestNotNull(TEXT("Removing cover restores the actual protection opportunity"), F.Proof());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtectionGunfireRuntimeTest,
	"ProjectVelkorran.Campaign.Echo.Protection.DroneProducer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtectionGunfireRuntimeTest::RunTest(const FString&)
{
	FProtectionWorld F;
	if (!TestTrue(TEXT("World"), F.Ready())) return false;
	auto* Observer = NewObject<USovProtectionRuntimeObserver>(F.Tarrik);
	Observer->Source = F.Threat; Observer->Target = F.Tarrik; Observer->bNestOnNextTargetResult = true;
	F.Generator->OnTarrikEchoAwarded.AddUniqueDynamic(Observer, &USovProtectionRuntimeObserver::EchoAwarded);
	auto* SourceASC = F.Threat->GetNarrativeAbilitySystemComponent();
	auto* TargetASC = F.Tarrik->GetNarrativeAbilitySystemComponent();
	SourceASC->OnDamageResolvedAsSource.AddUniqueDynamic(Observer, &USovProtectionRuntimeObserver::SourceResolved);
	TargetASC->OnDamageResolvedAsTarget.AddUniqueDynamic(Observer, &USovProtectionRuntimeObserver::TargetResolved);
	auto* Gunfire = F.Activate();
	if (!TestNotNull(TEXT("Native Drone ability activates"), Gunfire)) return false;
	Gunfire->FireGunBurstFromAim();
	TestEqual(TEXT("Both actual shot and nested unrelated hit reached GAS"), Observer->SourceResults.Num(), 2);
	if (Observer->SourceResults.Num() != 2) return false;
	const FSovDamageResult Nested = Observer->SourceResults[0];
	const FSovDamageResult Shot = Observer->SourceResults[1];
	TestTrue(TEXT("Nested damage carries a distinct context"), Nested.EffectContext.Get() != Shot.EffectContext.Get());
	TestEqual(TEXT("Shot context preserves original weapon SourceObject"), static_cast<const UObject*>(Shot.EffectContext.GetSourceObject()),
		static_cast<const UObject*>(F.Threat->GetWeapon()));
	TestEqual(TEXT("Shot context preserves SourceAbility CDO identity"), Shot.EffectContext.GetAbility(),
		static_cast<const UGameplayAbility*>(GetDefault<USovProtectionRuntimeGunfire>()));
	TestEqual(TEXT("Only exact accepted shot pays15"), F.Tarrik->TestEcho->GetEcho(), 15.f);
	TestEqual(TEXT("Exactly one protection callback"), Observer->ProtectionAwardCount, 1);
	SourceASC->OnDamageResolvedAsSource.Broadcast(Shot);
	SourceASC->OnDamageResolvedAsSource.Broadcast(Nested);
	TestEqual(TEXT("Post-delivery result replay cannot award"), F.Tarrik->TestEcho->GetEcho(), 15.f);
	Gunfire->Finish();
	Gunfire = F.Activate();
	if (!TestNotNull(TEXT("Second native shot activates"), Gunfire)) return false;
	Gunfire->FireGunBurstFromAim(); Gunfire->Finish();
	TestEqual(TEXT("Same threat has a five-second cooldown"), F.Tarrik->TestEcho->GetEcho(), 15.f);
	auto* OtherThreat = F.Character(0.f, 1);
	if (!TestNotNull(TEXT("Independent enemy source"), OtherThreat) || !TestNotNull(TEXT("Independent AI"), F.Focus(OtherThreat))) return false;
	F.Threat->SetActorEnableCollision(false); // Exchange sources without placing one inside the other's aim query.
	Gunfire = F.Activate(OtherThreat);
	if (!TestNotNull(TEXT("Other enemy's native shot"), Gunfire)) return false;
	Gunfire->FireGunBurstFromAim(); Gunfire->Finish();
	TestEqual(TEXT("Cooldown belongs to the enemy source"), F.Tarrik->TestEcho->GetEcho(), 30.f);
	OtherThreat->SetActorEnableCollision(false);
	F.Threat->SetActorEnableCollision(true);
	F.World->Tick(LEVELTICK_TimeOnly, 5.01f);
	Gunfire = F.Activate();
	if (!TestNotNull(TEXT("First source after cooldown"), Gunfire)) return false;
	const int32 AwardsBefore = Observer->ProtectionAwardCount;
	Gunfire->FireGunBurstFromAim(); Gunfire->Finish();
	TestEqual(TEXT("Original source pays again after five seconds"), Observer->ProtectionAwardCount, AwardsBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtectionDefenseRuntimeTest,
	"ProjectVelkorran.Campaign.Echo.Protection.DefenseAndReceipt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtectionDefenseRuntimeTest::RunTest(const FString&)
{
	FProtectionWorld F;
	if (!TestTrue(TEXT("World"), F.Ready())) return false;
	auto* ASC = F.Tarrik->GetNarrativeAbilitySystemComponent();
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Guarding);
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_PerfectGuard);
	auto* Gunfire = F.Activate();
	if (!TestNotNull(TEXT("Perfect Guard shot"), Gunfire)) return false;
	Gunfire->FireGunBurstFromAim(); Gunfire->Finish();
	TestTrue(TEXT("Real damage resolver confirms perfect Guard"), F.Tarrik->LastDamageResult.bPerfectDefense && F.Tarrik->LastDamageResult.bGuarded);
	TestEqual(TEXT("Perfect Guard prevents all body damage"), F.Tarrik->LastDamageResult.AppliedHealthDamage + F.Tarrik->LastDamageResult.AppliedShieldDamage, 0.f);
	TestEqual(TEXT("Zero-damage accepted Guard still protects the ally"), F.Tarrik->TestEcho->GetEcho(), 15.f);
	F.Tarrik->TestEcho->BeginEncounter(); // Independent native award scope.
	const float BeforeImmune = F.Tarrik->TestEcho->GetEcho();
	const int32 BeforeHits = F.Tarrik->ResolvedHitCount;
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable);
	Gunfire = F.Activate();
	if (!TestNotNull(TEXT("Invulnerability shot"), Gunfire)) return false;
	Gunfire->FireGunBurstFromAim(); Gunfire->Finish();
	TestEqual(TEXT("Invulnerability never emits an accepted damage receipt"), F.Tarrik->ResolvedHitCount, BeforeHits);
	TestEqual(TEXT("Invulnerability grants no protection credit"), F.Tarrik->TestEcho->GetEcho(), BeforeImmune);
	ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable);
	TStrongObjectPtr<USovProtectionInterceptReceipt> Receipt(F.Proof());
	if (!TestNotNull(TEXT("Geometrically verified receipt"), Receipt.Get())) return false;
	FGameplayEffectContextHandle Context = F.Threat->GetNarrativeAbilitySystemComponent()->MakeEffectContext();
	Context.AddInstigator(F.Threat, F.Threat);
	TestFalse(TEXT("Wrong source ASC cannot arm proof"), Receipt->ArmForDamage(Context, ASC, ASC));
	TestFalse(TEXT("Wrong victim ASC cannot arm proof"), Receipt->ArmForDamage(Context,
		F.Threat->GetNarrativeAbilitySystemComponent(), F.Ally->GetNarrativeAbilitySystemComponent()));
	TestTrue(TEXT("Exact source and target arm once"), Receipt->ArmForDamage(Context, F.Threat->GetNarrativeAbilitySystemComponent(), ASC));
	TestFalse(TEXT("Receipt cannot be rearmed"), Receipt->ArmForDamage(Context, F.Threat->GetNarrativeAbilitySystemComponent(), ASC));
	Receipt->ReceiveResult(F.Tarrik->LastDamageResult);
	AActor* OutThreat = nullptr; AActor* OutAlly = nullptr;
	TestFalse(TEXT("A real result with another context cannot consume proof"), Receipt->ConsumeForProtector(F.Tarrik,
		F.Tarrik->LastDamageResult, OutThreat, OutAlly));
	Receipt->Disarm();
	TestFalse(TEXT("Disarm permanently closes the receipt"), Receipt->ArmForDamage(Context, F.Threat->GetNarrativeAbilitySystemComponent(), ASC));
	ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Guarding);
	ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_PerfectGuard);
	auto* Observer = NewObject<USovProtectionRuntimeObserver>(F.Tarrik);
	F.Generator->OnTarrikEchoAwarded.AddUniqueDynamic(Observer, &USovProtectionRuntimeObserver::EchoAwarded);
	Gunfire = F.Activate();
	if (!TestNotNull(TEXT("Cancellation during reward delivery"), Gunfire)) return false;
	Gunfire->SetBurstSize(3); Observer->CancelOnAward = Gunfire;
	Gunfire->FireGunBurstFromAim();
	TestFalse(TEXT("Reward callback ended the active ability"), Gunfire->IsActive());
	const int32 HitsAtCancel = F.Tarrik->ResolvedHitCount;
	F.World->GetTimerManager().Tick(0.5f);
	TestEqual(TEXT("Old shot never schedules another burst after cancellation"), F.Tarrik->ResolvedHitCount, HitsAtCancel);
	return true;
}
#endif
