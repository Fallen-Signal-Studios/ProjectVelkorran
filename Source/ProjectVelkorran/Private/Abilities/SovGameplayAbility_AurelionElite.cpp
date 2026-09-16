// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_AurelionElite.h"

#include "AI/NPCDefinition.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Combat/SovTarrikPayloadSupport.h"
#include "Combat/SovThreatTargeting.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

USovGameplayAbility_AurelionEliteBase::USovGameplayAbility_AurelionEliteBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	bBotSelectionEnabled = true;
	bBotRequiresLineOfSight = true;
	bBotRequiresAttackToken = true;

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	ActivationBlockedTags.AddTag(NarrativeTags.State_IsDead);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Interacting);
	ActivationBlockedTags.AddTag(NarrativeTags.State_SequencerControlled);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(SovTags.State_Fatal);
	// A staggered boss stops attacking, exactly as a staggered ordinary enemy does.
	ActivationBlockedTags.AddTag(SovTags.State_Poise_Broken);
}

ASovEncounterDirector* USovGameplayAbility_AurelionEliteBase::ResolveOwningDirector() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!IsValid(Avatar) || !World) { return nullptr; }
	for (TActorIterator<ASovEncounterDirector> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->FindParticipantId(Avatar).IsNone()) { return *It; }
	}
	return nullptr;
}

SovAurelionElitePolicy::EPhase USovGameplayAbility_AurelionEliteBase::ResolvePhase() const
{
	// Read live, never cached: a phase component will own the monotonic guard when it lands, and the
	// policy already refuses to walk a phase backwards when given one.
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) { return SovAurelionElitePolicy::EPhase::First; }
	const float Maximum = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	if (!(Maximum > KINDA_SMALL_NUMBER)) { return SovAurelionElitePolicy::EPhase::First; }
	const float Current = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	return SovAurelionElitePolicy::PhaseForHealthFraction(Current / Maximum);
}

bool USovGameplayAbility_AurelionEliteBase::IsPhaseAdmitted() const
{
	return static_cast<uint8>(ResolvePhase()) >= static_cast<uint8>(RequiredPhase);
}

AActor* USovGameplayAbility_AurelionEliteBase::FindBossTarget(AActor* SourceActor) const
{
	const ASovEncounterDirector* const Director = ResolveOwningDirector();
	AActor* const Player = Director ? Director->GetEncounterPlayer() : nullptr;
	if (!IsValid(SourceActor) || !IsValid(Player)) { return nullptr; }
	// Acquisition only. Hostility and damage admission are resolved by the payload, not here.
	return SovThreatTargeting::CanTrack(SourceActor, Player) ? Player : nullptr;
}

void USovGameplayAbility_AurelionEliteBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	BossTarget.Reset();
	ANarrativeCharacter* const Avatar = ActorInfo ? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* const Target = FindBossTarget(Avatar);
	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !IsValid(Avatar) || !IsValid(Target)
		|| !IsPhaseAdmitted() || Now < NextAllowedActivationTime)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	BossTarget = Target;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || GetAvatarActorFromActorInfo() != Avatar)
	{
		return;
	}
	const bool bResolved = ExecuteBossPayload(Avatar, Target);
	if (World) { NextAllowedActivationTime = World->GetTimeSeconds() + FMath::Max(CooldownDuration, 0.f); }
	if (IsActive()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, !bResolved); }
}

USovGameplayAbility_AurelionEliteSlam::USovGameplayAbility_AurelionEliteSlam()
{
	MinimumAttackRange = 0.f;
	MaximumAttackRange = 560.f;
	CooldownDuration = 9.f;
	BotAttackPressure = ESovBotAttackPressure::Melee;
	BotSelectionPriority = 2.f;
	DefaultAttackDamage = SlamDamage;
	DefaultBotAttackRange = MaximumAttackRange;
	DefaultBotAttackFrequency = 1.f / FMath::Max(CooldownDuration, 1.f);
	// A slam does not need to see its target: it is the punishment for standing close.
	bBotRequiresLineOfSight = false;
}

bool USovGameplayAbility_AurelionEliteSlam::ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target)
{
	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const Source = GetAbilitySystemComponentFromActorInfo();
	if (!World || !IsValid(Source) || !IsValid(Avatar)) { return false; }
	const FVector Origin = Avatar->GetActorLocation();

	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.SetAbility(this); Context.AddInstigator(Avatar, Avatar); Context.AddOrigin(Origin);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAurelionEliteSlam), false, Avatar);
	SovTarrikPayload::IgnoreActorAndAttachments(Query, Avatar);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(SlamRadius), Query);

	TSet<UAbilitySystemComponent*> Seen;
	int32 Resolved = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UAbilitySystemComponent* const Victim = SovTarrikPayload::ResolveASC(Overlap.GetActor());
		if (!Victim || Seen.Contains(Victim)) { continue; }
		Seen.Add(Victim);
		// Hostility is judged from the elite's own team, so summoned adds are never caught by it.
		if (!SovTarrikPayload::Hostile(Source, Avatar, Victim)
			|| !SovTarrikPayload::Visible(World, Origin, Avatar, Avatar, Victim)) { continue; }
		const AActor* const VictimActor = Victim->GetAvatarActor();
		const float Distance = FVector::Distance(Origin, VictimActor->GetActorLocation());
		if (Distance > SlamRadius) { continue; }
		const float Falloff = FMath::Lerp(1.f, MinimumSlamDamageFraction, Distance / FMath::Max(SlamRadius, 1.f));
		if (SovTarrikPayload::ApplyDamage(Source, Avatar, Victim, Context, RadialDamageEffectClass,
			FSovGameplayTags::Get().Ability_NPC_AurelionElite_Slam, SlamDamage, SlamPoiseDamage, Falloff))
		{
			++Resolved;
		}
	}
	return Resolved > 0;
}

USovGameplayAbility_AurelionEliteLance::USovGameplayAbility_AurelionEliteLance()
{
	MinimumAttackRange = 600.f;
	MaximumAttackRange = 4000.f;
	CooldownDuration = 6.f;
	// Declared ranged: the encounter coordinator refuses this while the elite is offscreen and
	// unwarned, which is the intended readability rule rather than an obstacle to route around.
	BotAttackPressure = ESovBotAttackPressure::Ranged;
	BotSelectionPriority = 1.f;
	DefaultAttackDamage = LanceDamage;
	DefaultBotAttackRange = MaximumAttackRange;
	DefaultBotAttackFrequency = 1.f / FMath::Max(CooldownDuration, 1.f);
}

bool USovGameplayAbility_AurelionEliteLance::ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target)
{
	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const Source = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* const Victim = SovTarrikPayload::ResolveASC(Target);
	if (!World || !IsValid(Source) || !IsValid(Avatar) || !Victim) { return false; }

	const FVector Start = Avatar->GetActorLocation() + FVector(0.f, 0.f, Avatar->BaseEyeHeight);
	const FVector End = Target->GetActorLocation();
	if (FVector::Distance(Start, End) > MaximumAttackRange) { return false; }

	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAurelionEliteLance), false, Avatar);
	SovTarrikPayload::IgnoreActorAndAttachments(Query, Avatar);
	FHitResult Hit;
	const bool bBlocked = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(FMath::Max(LanceTraceRadius, 0.f)), Query);
	// A shot that stops on geometry hits that geometry, not the player behind it.
	if (bBlocked && SovTarrikPayload::ResolveASC(Hit.GetActor()) != Victim) { return false; }

	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.SetAbility(this); Context.AddInstigator(Avatar, Avatar); Context.AddOrigin(Start);
	return SovTarrikPayload::ApplyDamage(Source, Avatar, Victim, Context, LanceDamageEffectClass,
		FSovGameplayTags::Get().Ability_NPC_AurelionElite_Lance, LanceDamage, LancePoiseDamage);
}

USovGameplayAbility_AurelionEliteSummon::USovGameplayAbility_AurelionEliteSummon()
{
	MinimumAttackRange = 0.f;
	MaximumAttackRange = 6000.f;
	CooldownDuration = 22.f;
	RequiredPhase = SovAurelionElitePolicy::EPhase::Second;
	BotAttackPressure = ESovBotAttackPressure::Support;
	BotSelectionPriority = 3.f;
	DefaultAttackDamage = 0.f;
	DefaultBotAttackRange = MaximumAttackRange;
	DefaultBotAttackFrequency = 1.f / FMath::Max(CooldownDuration, 1.f);
	// Summoning is not an attack: it must not consume one of the encounter's melee attacker slots.
	bBotRequiresAttackToken = false;
	bBotRequiresLineOfSight = false;
	// Every transitive dependency of this definition is version controlled, so a summon behaves the
	// same on another machine. The definition's own class path supplies what is spawned.
	SummonDefinition = TSoftObjectPtr<UNPCDefinition>(FSoftObjectPath(TEXT("/Game/Aurelion/Enemies/NPC_AurelionEnforcer.NPC_AurelionEnforcer")));
}

int32 USovGameplayAbility_AurelionEliteSummon::GetLivingSummonCount() const
{
	int32 Living = 0;
	for (const TWeakObjectPtr<ASovNPCCharacterBase>& Summon : Summoned)
	{
		if (Summon.IsValid() && Summon->IsAlive()) { ++Living; }
	}
	return Living;
}

bool USovGameplayAbility_AurelionEliteSummon::ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target)
{
	UWorld* const World = GetWorld();
	ASovEncounterDirector* const Director = ResolveOwningDirector();
	if (!World || !IsValid(Director) || !IsValid(Avatar)) { return false; }

	Summoned.RemoveAll([](const TWeakObjectPtr<ASovNPCCharacterBase>& Summon)
		{ return !Summon.IsValid() || !Summon->IsAlive(); });
	const int32 Room = FMath::Min(SovAurelionElitePolicy::SummonCountForPhase(ResolvePhase()),
		MaximumLivingSummons - Summoned.Num());
	if (Room <= 0) { return false; }

	UNPCDefinition* const Definition = SummonDefinition.LoadSynchronous();
	UClass* const SummonClass = Definition ? Definition->NPCClassPath.LoadSynchronous() : nullptr;
	if (!IsValid(Definition) || !SummonClass || !SummonClass->IsChildOf(ASovNPCCharacterBase::StaticClass()))
	{
		// Loud rather than a silent uninitialised husk: an add without its definition never fights.
		UE_LOG(LogTemp, Warning, TEXT("Aurelion Elite summon has no resolvable authored add definition."));
		return false;
	}

	const FVector Origin = Avatar->GetActorLocation();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Room; ++Index)
	{
		const float Angle = (360.f / FMath::Max(Room, 1)) * Index;
		const FVector Offset = FRotator(0.f, Angle, 0.f).RotateVector(FVector::ForwardVector) * SummonDistance;
		const FTransform Placement(Avatar->GetActorRotation(), Origin + Offset);
		ASovNPCCharacterBase* const Add = World->SpawnActorDeferred<ASovNPCCharacterBase>(SummonClass, Placement,
			Director, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!IsValid(Add)) { continue; }
		// The definition must be set before BeginPlay, exactly as the director's own restore does.
		Add->SetNPCDefinition(Definition);
		Add->FinishSpawning(Placement);
		if (!IsValid(Add)) { continue; }
		Add->EnsureEncounterController();
		if (!IsValid(Add)) { continue; }
		// Attempt-scoped, never a victory participant: the encounter's required roster is fixed
		// before the fight starts, and killing summons is never required to win.
		Director->RegisterAttemptActor(Add);
		Summoned.Add(Add);
		++Spawned;
	}
	return Spawned > 0;
}
