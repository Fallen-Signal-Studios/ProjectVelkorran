// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/World.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Companions/SovCompanionComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"

ASovProtagonistCompanionCharacter::ASovProtagonistCompanionCharacter(const FObjectInitializer& Initializer) : Super(Initializer)
{
	Companion = CreateDefaultSubobject<USovCompanionComponent>(TEXT("SovCompanion"));
	Guard = CreateDefaultSubobject<USovGuardComponent>(TEXT("SovGuard"));
	Deflection = CreateDefaultSubobject<USovDeflectionComponent>(TEXT("SovDeflection"));
	Echo = CreateDefaultSubobject<USovEchoComponent>(TEXT("SovEcho"));
	Shield = CreateDefaultSubobject<USovShieldComponent>(TEXT("SovShield"));
	Poise = CreateDefaultSubobject<USovPoiseComponent>(TEXT("SovPoise"));
	AIControllerClass = ANarrativeNPCController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
bool ASovProtagonistCompanionCharacter::PrepareProxy(FGameplayTag Identity, FName CompanionId,
	const UNarrativeAbilitySystemComponent* OutgoingASC, const TArray<TSubclassOf<UGameplayAbility>>& CuratedClasses, FString& Reason)
{
	Reason.Reset(); const auto& Tags = FSovGameplayTags::Get();
	if (!HasAuthority() || HasActorBegunPlay() || bPrepared || CompanionId.IsNone() || !IsValid(OutgoingASC)
		|| !OutgoingASC->GetAvatarActor() || !OutgoingASC->GetSet<UNarrativeAttributeSetBase>()
		|| (Identity != Tags.Character_Player_Tarrik && Identity != Tags.Character_Player_Selene)
		|| !OutgoingASC->HasMatchingGameplayTag(Identity))
	{ Reason = TEXT("A proxy must copy a real, matching outgoing protagonist before spawning."); return false; }
	CopiedGrants.Reset(); TSet<UClass*> Seen;
	// Read only; the actual player's grant handles, progression state, costs and resources stay untouched.
	for (const auto& Class : CuratedClasses)
	{
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || Seen.Contains(Class.Get()))
		{ Reason = TEXT("Curated companion classes must be concrete and unique."); return false; }
		Seen.Add(Class.Get());
		const auto* Spec = const_cast<UNarrativeAbilitySystemComponent*>(OutgoingASC)->FindAbilitySpecFromClass(Class);
		if (!Spec || !Spec->Ability) { continue; } // Locked player choices are never silently unlocked for AI.
		FSovCompanionKitGrant Grant; Grant.Ability = Class; Grant.Level = Spec->Level; CopiedGrants.Add(Grant);
	}
	SavedMaxHealth = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	SavedHealth = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	SavedMaxShield = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute());
	SavedShield = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	SavedMaxStamina = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxStaminaAttribute());
	SavedStamina = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute());
	SavedMaxEcho = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxEchoAttribute());
	SavedEcho = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute());
	SavedMaxPoise = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxPoiseAttribute());
	SavedPoise = OutgoingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute());
	for (float Value : { SavedMaxHealth, SavedHealth, SavedMaxShield, SavedShield, SavedMaxStamina, SavedStamina, SavedMaxEcho, SavedEcho, SavedMaxPoise, SavedPoise })
	{ if (!FMath::IsFinite(Value) || Value < 0.f) { Reason = TEXT("Outgoing resources are invalid."); return false; } }
	if (SavedHealth <= 0.f || SavedMaxHealth <= 0.f) { Reason = TEXT("A defeated protagonist cannot become a living companion."); return false; }
	CompanionIdentity = Identity; Companion->CompanionId = CompanionId; Companion->CuratedAbilities.Reset();
	for (const auto& Grant : CopiedGrants) { Companion->CuratedAbilities.Add(Grant.Ability); }
	bEncounterOwned = true; bEncounterRestoreInitialization = true; NativeSaveGuid = FGuid::NewGuid(); bPrepared = true; return true;
}
void ASovProtagonistCompanionCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (bPrepared && AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(CompanionIdentity, 1, EGameplayTagReplicationState::TagAndCountToAll);
		Guard->InitializeWithAbilitySystem(AbilitySystemComponent); Deflection->InitializeWithAbilitySystem(AbilitySystemComponent);
		Echo->InitializeWithAbilitySystem(AbilitySystemComponent); Shield->InitializeWithAbilitySystem(AbilitySystemComponent); Poise->InitializeWithAbilitySystem(AbilitySystemComponent);
	}
}
bool ASovProtagonistCompanionCharacter::CompleteProxyInitialization()
{
	if (bProxyInitialized) { return true; }
	if (!bPrepared || bApplyingKit || !HasAuthority() || !IsEncounterSnapshotReady() || !AbilitySystemComponent
		|| AbilitySystemComponent->GetAvatarActor() != this || !GetController()) { return false; }
	TGuardValue<bool> Applying(bApplyingKit, true);
	auto* ASC = AbilitySystemComponent.Get();
	// Replace only grants duplicated by this explicit curated copy; unrelated default kit stays available but AI cannot select it.
	for (const auto& Grant : CopiedGrants)
	{
		if (auto* Existing = ASC->FindAbilitySpecFromClass(Grant.Ability))
		{ Existing->Level = Grant.Level; ASC->MarkAbilitySpecDirty(*Existing); continue; }
		OwnedKitHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(Grant.Ability, Grant.Level, INDEX_NONE, this)));
		if (IsActorBeingDestroyed() || ASC != AbilitySystemComponent || ASC->GetAvatarActor() != this) { return false; }
	}
	const auto Restore = [this, ASC](FGameplayAttribute Maximum, FGameplayAttribute Current, float MaxValue, float Value)
	{
		ASC->SetNumericAttributeBase(Maximum, MaxValue);
		if (IsActorBeingDestroyed() || ASC != AbilitySystemComponent || ASC->GetAvatarActor() != this) { return false; }
		ASC->SetNumericAttributeBase(Current, FMath::Clamp(Value, 0.f, MaxValue));
		return !IsActorBeingDestroyed() && ASC == AbilitySystemComponent && ASC->GetAvatarActor() == this;
	};
	if (!Restore(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), UNarrativeAttributeSetBase::GetHealthAttribute(), SavedMaxHealth, SavedHealth)
		|| !Restore(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), UNarrativeAttributeSetBase::GetShieldAttribute(), SavedMaxShield, SavedShield)
		|| !Restore(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(), SavedMaxStamina, SavedStamina)
		|| !Restore(UNarrativeAttributeSetBase::GetMaxEchoAttribute(), UNarrativeAttributeSetBase::GetEchoAttribute(), SavedMaxEcho, SavedEcho)
		|| !Restore(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), UNarrativeAttributeSetBase::GetPoiseAttribute(), SavedMaxPoise, SavedPoise)) { return false; }
	bProxyInitialized = !IsActorBeingDestroyed() && ASC->GetAvatarActor() == this;
	return bProxyInitialized;
}

bool ASovProtagonistCompanionCharacter::CaptureProxySnapshot(FName MissionId, FSovCompanionProxySnapshot& Snapshot, FString& Reason)
{
	Reason.Reset();
	if (!HasAuthority() || !bProxyInitialized || !AbilitySystemComponent || !IsAlive() || MissionId.IsNone())
	{ Reason = TEXT("The native companion is not ready for a save."); return false; }
	FSovCompanionProxySnapshot Candidate; Candidate.MissionId = MissionId; Candidate.Identity = CompanionIdentity;
	Candidate.CompanionId = Companion->CompanionId; Candidate.Grants = CopiedGrants;
	const auto* ASC = AbilitySystemComponent.Get(); auto& Values = Candidate.Resources;
	Values.Health = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()); Values.MaxHealth = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	Values.Shield = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()); Values.MaxShield = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute());
	Values.Stamina = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()); Values.MaxStamina = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxStaminaAttribute());
	Values.Poise = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()); Values.MaxPoise = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxPoiseAttribute());
	Values.Echo = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()); Values.MaxEcho = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxEchoAttribute());
	auto* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Values.IsValid() || !Save || !Save->CreateActorRecord(this, Candidate.ActorRecord))
	{ Reason = TEXT("The native companion record could not be captured."); return false; }
	Snapshot = MoveTemp(Candidate); return true;
}
bool ASovProtagonistCompanionCharacter::PrepareProxyFromSnapshot(const FSovCompanionProxySnapshot& Snapshot,
	const TArray<TSubclassOf<UGameplayAbility>>& Curated, FString& Reason)
{
	Reason.Reset(); const auto& Tags = FSovGameplayTags::Get();
	if (!HasAuthority() || HasActorBegunPlay() || bPrepared || !Snapshot.Resources.IsValid() || Snapshot.Resources.Health <= 0.f
		|| !Snapshot.ActorRecord.IsValid() || Snapshot.CompanionId.IsNone()
		|| (Snapshot.Identity != Tags.Character_Player_Tarrik && Snapshot.Identity != Tags.Character_Player_Selene))
	{ Reason = TEXT("The saved protagonist companion is invalid."); return false; }
	TSet<UClass*> Seen;
	for (const auto& Grant : Snapshot.Grants)
	{
		if (!Grant.Ability || Grant.Ability->HasAnyClassFlags(CLASS_Abstract) || Grant.Level <= 0 || Grant.Level > 100
			|| Seen.Contains(Grant.Ability.Get()) || !Curated.Contains(Grant.Ability))
		{ Reason = TEXT("A saved companion grant is outside the authored curated kit."); return false; }
		Seen.Add(Grant.Ability.Get());
	}
	CompanionIdentity = Snapshot.Identity; Companion->CompanionId = Snapshot.CompanionId; CopiedGrants = Snapshot.Grants;
	Companion->CuratedAbilities.Reset(); for (const auto& Grant : CopiedGrants) { Companion->CuratedAbilities.Add(Grant.Ability); }
	const auto& V = Snapshot.Resources;
	SavedHealth = V.Health; SavedMaxHealth = V.MaxHealth; SavedShield = V.Shield; SavedMaxShield = V.MaxShield;
	SavedStamina = V.Stamina; SavedMaxStamina = V.MaxStamina; SavedEcho = V.Echo; SavedMaxEcho = V.MaxEcho;
	SavedPoise = V.Poise; SavedMaxPoise = V.MaxPoise;
	bEncounterOwned = true; bEncounterRestoreInitialization = true;
	SetActorGUID_Implementation(Snapshot.ActorRecord.ActorGUID); bPrepared = true; return true;
}

void ASovProtagonistCompanionCharacter::SetProxyStaged(bool bStaged)
{
	bStagedForTransition = bStaged;
	SetActorHiddenInGame(bStaged); SetActorEnableCollision(!bStaged);
	if (auto* Visual = GetCharacterVisual()) { Visual->SetActorHiddenInGame(bStaged); }
}
void ASovProtagonistCompanionCharacter::OnCharacterVisualInitialized()
{
	Super::OnCharacterVisualInitialized(); SetProxyStaged(bStagedForTransition);
}
