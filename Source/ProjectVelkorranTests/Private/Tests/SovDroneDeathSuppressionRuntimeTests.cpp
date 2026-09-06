// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDroneDeathSuppressionTestFixtures.h"
#include "Tests/SovDroneContinuationTestFixtures.h"

#include "Components/CapsuleComponent.h"
#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

ASovDroneDeathSuppressionTestCharacter::ASovDroneDeathSuppressionTestCharacter(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USovDroneContinuationASC>(TEXT("AbilitySystemComponent")))
{
	bEnableDeathExplosionOnDeath = true;
	bEnableVerticalHoverVariation = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASovDroneDeathSuppressionTestCharacter::InitializeTestCombat()
{
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
	AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::ForwardNativeDeath);
}

ETeamAttitude::Type ASovDroneDeathSuppressionTestCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	return &Other == this ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

FGameplayTagContainer ASovDroneDeathSuppressionTestCharacter::GetFactions() const
{
	return FGameplayTagContainer();
}

void ASovDroneDeathSuppressionTestCharacter::ForwardNativeDeath(
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC, const bool bIsDead)
{
	if (bIsDead) { ++DeathNotifications; }
	ASovDroneNPCBase::HandleDeath_Implementation(KilledActor, KilledASC, bIsDead);
}

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FDroneDeathWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovDroneDeathSuppressionTestCharacter* Source = nullptr;
		ASovAxiomRuntimeTestCharacter* Target = nullptr;
		AActor* Replacement = nullptr;
		uint64 TimerFrame = GFrameCounter;
		FDroneDeathWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Source = World->SpawnActor<ASovDroneDeathSuppressionTestCharacter>(
				ASovDroneDeathSuppressionTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
			Target = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(150., 0., 0.), FRotator::ZeroRotator, Params);
			// Narrative intentionally ignores non-character avatar replacements.
			Replacement = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(2000., 0., 0.), FRotator::ZeroRotator, Params);
			if (Source) { Source->InitializeTestCombat(); }
			if (Target) { Target->InitializeTestCombat(2); }
		}
		~FDroneDeathWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		bool Valid() const { return World && Source && Target && Replacement; }
		void Tick(float Seconds)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
			World->GetTimerManager().Tick(Seconds);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneRejectedFatalSuppressionTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.RejectedFatalPreservesOrdinaryDeathExplosion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneRejectedFatalSuppressionTest::RunTest(const FString& Parameters)
{
	for (const bool bRejectCommittedFatal : {false, true})
	{
		FDroneDeathWorld F;
		if (!TestTrue(TEXT("Native DroneNPC world is available"), F.Valid())) { return false; }
		auto* ASC = Cast<USovDroneContinuationASC>(F.Source->GetNarrativeAbilitySystemComponent());
		auto* TargetASC = F.Target->GetNarrativeAbilitySystemComponent();
		if (!TestNotNull(TEXT("Native NPC uses the spec-construction seam"), ASC)) { return false; }
		if (bRejectCommittedFatal)
		{
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovDroneContinuationExploder::StaticClass(), 1));
			auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			auto* Ability = Spec ? Cast<USovDroneContinuationExploder>(Spec->GetPrimaryInstance()) : nullptr;
			if (!Ability || !TestTrue(TEXT("Native drone starts self destruct"), ASC->TryActivateAbility(Handle, false))) { return false; }
			bool bFatalSpecCallback = false;
			const uint64 OriginalActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
			ASC->SpecsBeforeCallback = 1; // Allow the one outward target spec, reject the fatal self spec.
			ASC->OnMakeSpec = [&]()
			{
				bFatalSpecCallback = true;
				ASC->InitAbilityActorInfo(F.Replacement, F.Replacement);
				ASC->InitAbilityActorInfo(F.Source, F.Source);
			};
			Ability->StartSelfDestructRun();
			F.Tick(.25f);
			TestTrue(TEXT("Fatal spec callback performed actor-info ABA"), bFatalSpecCallback);
			TestTrue(TEXT("Narrative admitted both avatar transitions"), ASC->GetCombatActorInfoEpoch() >= OriginalActorInfoEpoch + 2);
			TestEqual(TEXT("Rejected fatal leaves native DroneNPC alive"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
			TestEqual(TEXT("No native death callback occurred for rejected fatal"), F.Source->DeathNotifications, 0);
			TestTrue(TEXT("Outward blast was already committed"), TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()) < 100.f);
			TestFalse(TEXT("Stale self destruct is no longer active"), Ability->IsActive());
			// Retire the committed visual. Its independent duplicate-blast guard must
			// not hide a persistent suppression leak on a later unrelated death.
			for (TActorIterator<ASovReformationDroneSelfDestructPresentation> It(F.World); It; ++It)
			{
				if (It->GetOwner() == F.Source) { It->Destroy(); }
			}
			TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
		}
		// Real Narrative damage -> out-of-health -> death delegate -> DroneNPC blast.
		// The unmodified control establishes that the world/overlap/death path works.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(F.Source, F.Source);
		FGameplayEffectSpecHandle Fatal = ASC->MakeOutgoingSpec(USovGameplayEffect_ReformationDroneDamage::StaticClass(), 1.f, Context);
		if (!TestTrue(TEXT("Ordinary fatal spec is valid"), Fatal.IsValid())) { return false; }
		Fatal.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Fatal);
		Fatal.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 101.f);
		ASC->ApplyGameplayEffectSpecToSelf(*Fatal.Data.Get());
		TestTrue(TEXT("Ordinary fatal effect enters Narrative death"), ASC->IsDead());
		TestEqual(TEXT("Native drone receives its ordinary death exactly once"), F.Source->DeathNotifications, 1);
		TestTrue(bRejectCommittedFatal
			? TEXT("Rejected fatal spec did not suppress the later native death explosion")
			: TEXT("Control ordinary native death explosion damages its target"),
			TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()) < 100.f);
	}
	return true;
}
#endif
