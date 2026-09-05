// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterPolicy.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SovEchoComponent.h"
#include "Exertion/SovExertionComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeSavableComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "Misc/ScopeExit.h"

bool FSovCombatResourceSnapshot::IsValid() const
{
	return SovEncounterPolicy::ValidResource(Health, MaxHealth)
		&& SovEncounterPolicy::ValidResource(Shield, MaxShield)
		&& SovEncounterPolicy::ValidResource(Stamina, MaxStamina)
		&& SovEncounterPolicy::ValidResource(Poise, MaxPoise)
		&& SovEncounterPolicy::ValidResource(Echo, MaxEcho);
}

bool FSovProtagonistSnapshot::IsValid() const
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	return SchemaVersion == 1 && (ProtagonistTag == Tags.Character_Player_Tarrik || ProtagonistTag == Tags.Character_Player_Selene)
		&& !PawnClass.IsNull() && !PlayerDefinition.IsNull() && Resources.IsValid() && PawnRecord.IsValid()
		&& Resources.Health > 0.f
		&& PawnRecord.ActorSoftClass.ToSoftObjectPath() == PawnClass.ToSoftObjectPath()
		&& !SkillTreeRecord.ComponentName.IsNone();
}

bool USovEncounterSnapshotLibrary::CaptureResources(UAbilitySystemComponent* ASC, FSovCombatResourceSnapshot& OutSnapshot)
{
	if (!IsValid(ASC) || !ASC->GetSet<UNarrativeAttributeSetBase>()) { return false; }
	FSovCombatResourceSnapshot Result;
#define SOV_CAPTURE_RESOURCE(Name) Result.Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::Get##Name##Attribute()); Result.Max##Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute());
	SOV_CAPTURE_RESOURCE(Health)
	SOV_CAPTURE_RESOURCE(Shield)
	SOV_CAPTURE_RESOURCE(Stamina)
	SOV_CAPTURE_RESOURCE(Poise)
	SOV_CAPTURE_RESOURCE(Echo)
#undef SOV_CAPTURE_RESOURCE
	if (!Result.IsValid()) { return false; }
	OutSnapshot = Result;
	return true;
}

bool USovEncounterSnapshotLibrary::RestoreResources(UAbilitySystemComponent* ASC, const FSovCombatResourceSnapshot& Snapshot)
{
	if (!IsValid(ASC) || !IsValid(ASC->GetOwnerActor()) || !ASC->GetOwnerActor()->HasAuthority()
		|| !ASC->GetSet<UNarrativeAttributeSetBase>() || !Snapshot.IsValid()) { return false; }
	AActor* Avatar = ASC->GetAvatarActor();
	USovShieldComponent* ShieldComponent = Avatar ? Avatar->FindComponentByClass<USovShieldComponent>() : nullptr;
	USovPoiseComponent* PoiseComponent = Avatar ? Avatar->FindComponentByClass<USovPoiseComponent>() : nullptr;
	if (ShieldComponent) { ShieldComponent->SetCheckpointRestoreInProgress(true); }
	if (PoiseComponent) { PoiseComponent->SetCheckpointRestoreInProgress(true); }
	ON_SCOPE_EXIT
	{
		if (IsValid(ShieldComponent)) { ShieldComponent->SetCheckpointRestoreInProgress(false); }
		if (IsValid(PoiseComponent)) { PoiseComponent->SetCheckpointRestoreInProgress(false); }
	};
	const auto StillOwnsAvatar = [&]() { return IsValid(ASC) && IsValid(Avatar) && ASC->GetAvatarActor() == Avatar; };
	// Generic Narrative NPCs initialize attributes on revive; campaign players retain
	// their existing maxima and grants. In either case restore explicit currents last.
	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC))
	{
		if (NarrativeASC->IsDead() && Snapshot.Health > 0.f) { NarrativeASC->Revive(); }
	}
	if (!StillOwnsAvatar()) { return false; }
#define SOV_RESTORE_RESOURCE(Name) { const float Desired = SovEncounterPolicy::ClampRestoredResource(Snapshot.Name, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute())); ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), Desired); if (!StillOwnsAvatar() || !FMath::IsNearlyEqual(ASC->GetNumericAttribute(UNarrativeAttributeSetBase::Get##Name##Attribute()), Desired, 0.01f)) { return false; } }
	SOV_RESTORE_RESOURCE(Shield)
	SOV_RESTORE_RESOURCE(Stamina)
	SOV_RESTORE_RESOURCE(Poise)
	SOV_RESTORE_RESOURCE(Health)
	SOV_RESTORE_RESOURCE(Echo)
#undef SOV_RESTORE_RESOURCE
	if (Avatar)
	{
		if (USovExertionComponent* Exertion = Avatar->FindComponentByClass<USovExertionComponent>()) { Exertion->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovHealthRechargeComponent* Health = Avatar->FindComponentByClass<USovHealthRechargeComponent>()) { Health->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovShieldComponent* Shield = Avatar->FindComponentByClass<USovShieldComponent>()) { Shield->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovPoiseComponent* Poise = Avatar->FindComponentByClass<USovPoiseComponent>()) { Poise->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovEchoComponent* Echo = Avatar->FindComponentByClass<USovEchoComponent>())
		{
			Echo->RestoreEchoFromCheckpoint(Snapshot.Echo);
		}
	}
	return StillOwnsAvatar();
}

bool USovEncounterSnapshotLibrary::CaptureComponent(UActorComponent* Component, FNarrativeSaveComponent& OutRecord)
{
	if (!IsValid(Component) || !Component->Implements<UNarrativeSavableComponent>()) { return false; }
	FNarrativeSaveComponent Result;
	Result.ComponentName = Component->GetFName();
	INarrativeSavableComponent::Execute_PrepareForSave(Component);
	if (!IsValid(Component) || Component->GetFName() != Result.ComponentName) { return false; }
	FMemoryWriter Writer(Result.ByteData);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	Archive.ArIsSaveGame = true;
	Component->Serialize(Archive);
	if (Archive.IsError()) { return false; }
	OutRecord = MoveTemp(Result);
	return true;
}

bool USovEncounterSnapshotLibrary::RestoreComponent(UActorComponent* Component, const FNarrativeSaveComponent& Record)
{
	if (!IsValid(Component) || !Component->GetOwner() || !Component->GetOwner()->HasAuthority()
		|| Component->GetFName() != Record.ComponentName || !Component->Implements<UNarrativeSavableComponent>()) { return false; }
	FMemoryReader Reader(Record.ByteData);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;
	Component->Serialize(Archive);
	if (Archive.IsError()) { return false; }
	INarrativeSavableComponent::Execute_Load(Component);
	return true;
}
