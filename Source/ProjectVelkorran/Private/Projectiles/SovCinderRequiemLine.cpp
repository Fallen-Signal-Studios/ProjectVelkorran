// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Projectiles/SovCinderRequiemLine.h"
#include "NiagaraFunctionLibrary.h"
#include "Combat/SovCinderLineMath.h"
#include "Combat/SovTarrikPayloadSupport.h"
#include "Sovereign/SovEnvironmentDamage.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ASovCinderRequiemLine::ASovCinderRequiemLine()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LineOrigin"));
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(30.f);
}
void ASovCinderRequiemLine::InitializeLine(UAbilitySystemComponent* Source, AActor* InstigatorActor,
	const FGameplayEffectContextHandle& Context, FGameplayTag AbilityTag,
	const FVector& Start, const FVector& End, float Spacing, float Interval, float Radius,
	TSubclassOf<UGameplayEffect> DamageEffect, TSubclassOf<UGameplayEffect> BurnEffect,
	float Damage, float Poise, float BurnDamage, float BurnDuration)
{
	if (!HasAuthority() || bInitialized || bStarted || !IsValid(Source) || !IsValid(InstigatorActor)
		|| Start.ContainsNaN() || End.ContainsNaN() || !FMath::IsFinite(Interval) || Interval < 0.01f
		|| !FMath::IsFinite(Radius) || Radius <= 0.f || !FMath::IsFinite(Damage) || Damage <= 0.f
		|| !FMath::IsFinite(Poise) || Poise < 0.f || !FMath::IsFinite(BurnDamage) || BurnDamage <= 0.f
		|| !FMath::IsFinite(BurnDuration) || BurnDuration <= 0.f) { return; }
	const int32 Count = SovCinderLine::NodeCount(FVector::Distance(Start, End), Spacing);
	if (Count <= 0) { return; }
	SourceASC = Source; SourceActor = InstigatorActor; SourceContext = Context.Duplicate();
	EchoAbilityTag = AbilityTag; BlastRadius = Radius; DetonationInterval = Interval;
	DamageEffectClass = SovTarrikPayload::DamageEffect(DamageEffect);
	BurnEffectClass = SovTarrikPayload::BurnEffect(BurnEffect);
	BaseDamage = Damage; PoiseDamage = Poise; BurnDamagePerTick = BurnDamage; BurnSeconds = BurnDuration;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		DetonationPoints.Add(FMath::Lerp(Start, End, SovCinderLine::NodeAlpha(Index, Count)));
	}
	bInitialized = true;
}
void ASovCinderRequiemLine::StartLine()
{
	if (!HasAuthority() || bStarted) { return; }
	bStarted = true;
	if (!bInitialized || !GetWorld()) { Destroy(); return; }
	SetLifeSpan(DetonationPoints.Num() * DetonationInterval + 2.f);
	GetWorld()->GetTimerManager().SetTimer(DetonationTimer, this, &ThisClass::DetonateNext, DetonationInterval, true);
	DetonateNext();
}
void ASovCinderRequiemLine::DetonateNext()
{
	if (!HasAuthority() || !bInitialized || !DetonationPoints.IsValidIndex(NextNode)) { return; }
	if (!SourceASC.IsValid() || !SourceActor.IsValid() || SourceASC->GetAvatarActor() != SourceActor.Get()) { Destroy(); return; }
	const int32 Node = NextNode++; // claim before callbacks, including damage/death notifications
	const FVector Origin = DetonationPoints[Node];
	SetActorLocation(Origin);
	UAbilitySystemComponent* Source = SourceASC.Get();
	AActor* InstigatorActor = SourceActor.Get();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic); Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCinderRequiemDetonation), false, this);
	SovTarrikPayload::IgnoreActorAndAttachments(Query, InstigatorActor);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(BlastRadius), Query);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (IsActorBeingDestroyed() || !SourceASC.IsValid() || !SourceActor.IsValid()) { return; }
		UAbilitySystemComponent* Target = SovTarrikPayload::ResolveASC(Overlap.GetActor());
		if (!Target || DamagedTargets.Contains(TWeakObjectPtr<UAbilitySystemComponent>(Target)) || !SovTarrikPayload::Hostile(Source, InstigatorActor, Target)
			|| FVector::DistSquared(Origin, Target->GetAvatarActor()->GetActorLocation()) > FMath::Square(BlastRadius)
			|| !SovTarrikPayload::Visible(GetWorld(), Origin, InstigatorActor, this, Target)) { continue; }
		DamagedTargets.Add(TWeakObjectPtr<UAbilitySystemComponent>(Target)); // one line packet per target, including defended attempts
		FGameplayEffectContextHandle Context = SourceContext.Duplicate();
		Context.AddInstigator(InstigatorActor, this); Context.AddOrigin(Origin);
		if (SovTarrikPayload::ApplyDamage(Source, InstigatorActor, Target, Context, DamageEffectClass,
			EchoAbilityTag, BaseDamage, PoiseDamage, 1.f, nullptr, true))
		{
			SovTarrikPayload::ApplyBurn(Source, Target, Context, BurnEffectClass, EchoAbilityTag, BurnDamagePerTick, BurnSeconds);
		}
	}
	// Scenery follows the character packets and, like them, receives one packet per line.
	if (!IsActorBeingDestroyed() && SourceActor.IsValid())
	{
		for (const TWeakObjectPtr<AActor>& Damaged : DamagedScenery) { if (Damaged.IsValid()) { Query.AddIgnoredActor(Damaged.Get()); } }
		TArray<AActor*> NewlyDamaged;
		SovEnvironmentDamage::ApplyRadial(InstigatorActor, Origin, BlastRadius, BaseDamage, 1.f, true, Query, &NewlyDamaged);
		for (AActor* Actor : NewlyDamaged) { DamagedScenery.Add(Actor); }
	}
	LastDetonatedNode = Node;
	ForceNetUpdate();
	OnRep_LastDetonatedNode();
	if (NextNode >= DetonationPoints.Num())
	{
		GetWorld()->GetTimerManager().ClearTimer(DetonationTimer);
		SetLifeSpan(2.f);
	}
}
void ASovCinderRequiemLine::OnRep_LastDetonatedNode()
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	const int32 Last = FMath::Min(LastDetonatedNode, DetonationPoints.Num() - 1);
	while (LastPresentedNode < Last)
	{
		++LastPresentedNode;
		if (DetonationNiagaraSystem)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DetonationNiagaraSystem,
				DetonationPoints[LastPresentedNode], FRotator::ZeroRotator, FVector(0.4f));
		}
		ReceiveLineDetonation(DetonationPoints[LastPresentedNode], BlastRadius, LastPresentedNode);
	}
}
void ASovCinderRequiemLine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ASovCinderRequiemLine, DetonationPoints, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ASovCinderRequiemLine, BlastRadius, COND_InitialOnly);
	DOREPLIFETIME(ASovCinderRequiemLine, LastDetonatedNode);
}
void ASovCinderRequiemLine::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DetonationTimer); }
	Super::EndPlay(Reason);
}
