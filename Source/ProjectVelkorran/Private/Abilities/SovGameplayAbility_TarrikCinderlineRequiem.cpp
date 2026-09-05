// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Combat/SovCinderLineMath.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Combat/SovTarrikPayloadSupport.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Projectiles/SovCinderRequiemLine.h"
#include "Weapons/WeaponVisual.h"

USovGameplayAbility_TarrikCinderlineRequiem::USovGameplayAbility_TarrikCinderlineRequiem()
{
	const auto& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = EchoCost = 90.f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderlineRequiem;
	FGameplayTagContainer AssetTags = GetAssetTags(); AssetTags.AddTag(EchoSpendTag); SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderlineRequiem;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	PenetratingDamageEffectClass = LineDetonationEffectClass = USovGameplayEffect_CinderJudgementDamage::StaticClass();
	BurnEffectClass = USovGameplayEffect_CinderGrenadeBurn::StaticClass();
	LineClass = ASovCinderRequiemLine::StaticClass();
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderlineRequiemName", "Cinderline Requiem");
	AbilityDescription = NSLOCTEXT("SovTarrikEcho", "CinderlineRequiemDescription",
		"Release a penetrating Cinderline shot that marks its path, then erupts into a chained burning line with extreme Poise pressure.");
}

bool USovGameplayAbility_TarrikCinderlineRequiem::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(MaximumRange) && MaximumRange > 0.f && MaximumRange <= 30000.f
		&& FMath::IsFinite(LineDetonationRadius) && LineDetonationRadius > 0.f && LineDetonationRadius <= 3000.f
		&& FMath::IsFinite(PenetratingDamage) && PenetratingDamage > 0.f
		&& FMath::IsFinite(PenetratingPoiseDamage) && PenetratingPoiseDamage >= 0.f
		&& FMath::IsFinite(LineDetonationDamage) && LineDetonationDamage > 0.f
		&& FMath::IsFinite(LineDetonationPoiseDamage) && LineDetonationPoiseDamage >= 0.f
		&& FMath::IsFinite(BurnDamagePerTick) && BurnDamagePerTick > 0.f
		&& FMath::IsFinite(BurnDuration) && BurnDuration > 0.f
		&& FMath::IsFinite(LineDetonationSpacing) && LineDetonationSpacing >= 1.f
		&& FMath::IsFinite(LineDetonationInterval) && LineDetonationInterval >= 0.01f && LineDetonationInterval <= 1.f
		&& !FallbackMuzzleOffset.ContainsNaN() && FallbackMuzzleOffset.SizeSquared() <= FMath::Square(500.f)
		&& SovCinderLine::ValidTiming(PayloadReleaseDelay, PostReleaseRecovery, MaximumActiveDuration);
}
void USovGameplayAbility_TarrikCinderlineRequiem::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bRequiemReleaseAttempted = false;
	const uint64 Activation = GetTarrikActivationSerial() + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!bRequiemReleaseAttempted) { ArmTarrikPayload(PayloadReleaseDelay, Activation); }
}
void USovGameplayAbility_TarrikCinderlineRequiem::ExecuteAutomaticTarrikPayload()
{
	const uint64 Activation = GetTarrikActivationSerial();
	if (!ReleaseCinderlineRequiemFromAim() && IsActive() && GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
}

bool USovGameplayAbility_TarrikCinderlineRequiem::ReleaseCinderlineRequiemFromAim()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || bRequiemReleaseAttempted) { return false; }
	bRequiemReleaseAttempted = true;
	if (!IsTarrikReleaseContextValid() || !HasRequiredPayloadConfiguration() || !GetWorld()) { FinishEchoAbility(true); return false; }
	const uint64 Activation = GetTarrikActivationSerial();
	UAbilitySystemComponent* Source = CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	FVector Eye = Avatar->GetActorLocation(); FRotator Aim = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(Eye, Aim);
	if (AController* Controller = GetOwningController()) { Aim = Controller->GetControlRotation(); }
	if (Eye.ContainsNaN() || Aim.ContainsNaN()) { FinishEchoAbility(true); return false; }
	if (FVector::DistSquared(Eye, Avatar->GetActorLocation()) > FMath::Square(500.f)) { Eye = Avatar->GetActorLocation(); }
	FVector Start = Avatar->GetActorTransform().TransformPositionNoScale(FallbackMuzzleOffset);
	if (auto* Character = Cast<ANarrativeCharacter>(Avatar))
	{
		if (AWeaponVisual* Visual = Character->GetWieldedWeaponVisual(true))
		{
			USkeletalMeshComponent* Mesh = Visual->WeaponMesh;
			if (Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
			{
				const FVector Socket = Mesh->GetSocketLocation(MuzzleSocketName);
				if (!Socket.ContainsNaN() && FVector::DistSquared(Socket, Avatar->GetActorLocation()) <= FMath::Square(500.f)) { Start = Socket; }
			}
		}
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCinderRequiem), true, Avatar);
	Query.bReturnPhysicalMaterial = true;
	const ECollisionChannel WeaponChannel = UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel;
	SovTarrikPayload::IgnoreActorAndAttachments(Query, Avatar);
	FHitResult MuzzleBridge;
	if (World->LineTraceSingleByChannel(MuzzleBridge, Eye, Start, ECC_Visibility, Query)) { Start = Eye; }
	FHitResult AimHit;
	const FVector AimEnd = Eye + Aim.Vector() * MaximumRange;
	const bool bAimHit = World->LineTraceSingleByChannel(AimHit, Eye, AimEnd, WeaponChannel, Query);
	FVector Direction = ((bAimHit ? FVector(AimHit.ImpactPoint) : AimEnd) - Start).GetSafeNormal();
	// A close eye hit behind a forward muzzle must never reverse the shot.
	if (Direction.IsNearlyZero() || FVector::DotProduct(Direction, Aim.Vector()) <= 0.f) { Direction = Aim.Vector(); }
	const FVector UnblockedEnd = Start + Direction * MaximumRange;
	FVector End = UnblockedEnd;
	TArray<FHitResult> PenetratedHits;
	TSet<UAbilitySystemComponent*> Seen;

	// Channel traces stop at the first blocker. Ignore each resolved character and
	// retrace, retaining the first real world obstacle and the original hit bone.
	for (int32 TraceIndex = 0; TraceIndex < 128; ++TraceIndex)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, UnblockedEnd, WeaponChannel, Query)) { End = UnblockedEnd; break; }
		UAbilitySystemComponent* Target = SovTarrikPayload::ResolveASC(Hit.GetActor());
		if (!Target || !Target->GetAvatarActor() || Target == Source)
		{
			End = FVector(Hit.ImpactPoint) - Direction * 2.f;
			break;
		}
		Query.AddIgnoredActor(Hit.GetActor());
		SovTarrikPayload::IgnoreActorAndAttachments(Query, Target->GetAvatarActor());
		if (!Seen.Contains(Target))
		{
			Seen.Add(Target);
			if (SovTarrikPayload::Hostile(Source, Avatar, Target)) { PenetratedHits.Add(Hit); }
		}
		// At the hard work bound, never declare an untraced line clear.
		End = FVector(Hit.ImpactPoint);
	}
	if (FVector::DotProduct(End - Start, Direction) < 0.f) { End = Start; }

	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.SetAbility(this); Context.AddInstigator(Avatar, Avatar); Context.AddSourceObject(GetCurrentSourceObject());
	TSubclassOf<ASovCinderRequiemLine> PayloadClass = LineClass.Get() && !LineClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)
		? LineClass : ASovCinderRequiemLine::StaticClass();
	const FTransform SpawnTransform(Direction.ToOrientationQuat(), Start);
	auto* Line = World->SpawnActorDeferred<ASovCinderRequiemLine>(PayloadClass, SpawnTransform,
		Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Line))
	{
		Line = World->SpawnActorDeferred<ASovCinderRequiemLine>(ASovCinderRequiemLine::StaticClass(), SpawnTransform,
			Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	if (!Line) { FinishEchoAbility(true); return false; }
	Line->InitializeLine(Source, Avatar, Context, EchoSpendTag, Start, End, LineDetonationSpacing, LineDetonationInterval,
		LineDetonationRadius, LineDetonationEffectClass, BurnEffectClass, LineDetonationDamage, LineDetonationPoiseDamage, BurnDamagePerTick, BurnDuration);
	UGameplayStatics::FinishSpawningActor(Line, SpawnTransform);
	if (!IsValid(Line) || Line->IsActorBeingDestroyed()) { FinishEchoAbility(true); return false; }
	int32 Resolved = 0;
	for (const FHitResult& Hit : PenetratedHits)
	{
		if (!IsValid(Source) || !IsValid(Avatar) || Source->GetAvatarActor() != Avatar) { break; }
		FGameplayEffectContextHandle DirectContext = Context.Duplicate();
		DirectContext.AddHitResult(Hit, true); DirectContext.AddOrigin(Hit.ImpactPoint);
		if (SovTarrikPayload::ApplyDamage(Source, Avatar, SovTarrikPayload::ResolveASC(Hit.GetActor()), DirectContext,
			PenetratingDamageEffectClass, EchoSpendTag, PenetratingDamage, PenetratingPoiseDamage)) { ++Resolved; }
	}
	if (IsValid(Line) && !Line->IsActorBeingDestroyed()) { Line->StartLine(); }
	if (IsActive() && GetTarrikActivationSerial() == Activation)
	{
		ReceiveCinderlineRequiemReleased(Start, End, Resolved);
		BeginTarrikRecovery(PostReleaseRecovery, Activation);
	}
	return true;
}
