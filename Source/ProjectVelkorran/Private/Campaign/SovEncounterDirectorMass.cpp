// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovCampaignMassPolicy.h"
#include "Campaign/SovCampaignMassProxy.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/PoseSnapshot.h"
#include "Animation/AnimSequence.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Components/SovDismembermentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Effects/SovGameplayEffect_CorruptionBand.h"
#include "Components/SovCorruptionComponent.h"
#include "NarrativeSavableComponent.h"
#include "Items/NarrativeItem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTemplate.h"
#include "MassCommandBuffer.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationFragments.h"
#include "AI/Mass/Peds/MassPedRepresentationSubsystem.h"
#include "MassCommands.h"
#include "NarrativeGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	FGuid MassItemOwner(const ASovNPCCharacterBase* NPC, const UObject* Source)
	{
		const auto* Item = Cast<UNarrativeItem>(Source);
		return Item && Item->GetOwningNarrativeCharacter() == NPC ? Item->ItemGUID : FGuid();
	}
	FName MassEffectOwner(const ASovNPCCharacterBase* NPC, const FActiveGameplayEffect& Effect)
	{
		const auto* Component = Cast<UActorComponent>(Effect.Spec.GetContext().GetSourceObject());
		if (!Component && Effect.Spec.Def->IsA<USovGameplayEffect_CorruptionBand>()) { Component = NPC->FindComponentByClass<USovCorruptionComponent>(); }
		return Component && Component->GetOwner() == NPC && Component->Implements<UNarrativeSavableComponent>() ? Component->GetFName() : NAME_None;
	}
}

bool ASovEncounterDirector::IsParticipantMassRepresented(FName Id) const
{ return MassParticipants.ContainsByPredicate([Id](const auto& Record) { return Record.NPC.ParticipantId == Id; }); }

bool ASovEncounterDirector::HasLiveMassIdentity(FName Id) const
{
	auto* Entities = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	return Entities && MassEntities.Contains(Id) && Entities->GetMutableEntityManager().IsEntityValid(MassEntities.FindRef(Id));
}

bool ASovEncounterDirector::CaptureMassTransfer(ASovNPCCharacterBase* NPC, FSovCampaignMassState& Out, FString& Error) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SovEncounter_CaptureMassTransfer);
	auto* ASC = NPC ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
	if (!IsValid(NPC) || !ASC || ASC->GetAvatarActor() != NPC || NPC->IsHidden()
		|| (NPC->GetCharacterMovement() && (NPC->GetCharacterMovement()->IsFalling() || !NPC->GetVelocity().IsNearlyZero(1.f))))
	{ Error = TEXT("Mass conversion requires a visible, grounded, stationary initialized participant."); return false; }
	FSovCampaignMassState Result; Result.Factions = NPC->GetFactions(); ASC->GetOwnedGameplayTags(Result.OwnedTags);
	for (FGameplayTag Tag : Result.OwnedTags) { Result.TagCounts.Add(Tag, ASC->GetTagCount(Tag)); }
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() || Spec.PendingRemove || !Spec.Ability)
		{ Error = TEXT("An active, pending-removal or invalid GAS grant cannot convert."); return false; }
		FSovCampaignMassAbility& Grant = Result.Abilities.AddDefaulted_GetRef();
		Grant.Class = Spec.Ability->GetClass(); Grant.Level = Spec.Level; Grant.InputID = Spec.InputID;
		Grant.DynamicTags = Spec.GetDynamicSpecSourceTags();
		Grant.OwnerItem = MassItemOwner(NPC, Spec.SourceObject.Get());
		if (IsValid(Spec.SourceObject.Get()) && Spec.SourceObject.Get() != NPC && !Grant.OwnerItem.IsValid())
		{ Error = TEXT("A GAS ability source has no stable owned inventory identity."); return false; }
	}
	for (FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		const AActor* Source = Effect ? Effect->Spec.GetContext().GetOriginalInstigator() : nullptr;
		if (!Effect || !Effect->Spec.Def || !SovCampaignMassPolicy::TransferableEffect(Effect->Spec.GetDuration(), Effect->Spec.GetPeriod(), !Source || Source == NPC)
			|| Effect->bIsInhibited || !Effect->Spec.TargetEffectSpecs.IsEmpty() || !Effect->Spec.Def->Executions.IsEmpty())
		{ Error = TEXT("Timed, periodic, external-source or execution effects require an actor; conversion was not performed."); return false; }
		FSovCampaignMassEffect& Saved = Result.Effects.AddDefaulted_GetRef();
		Saved.Class = Effect->Spec.Def->GetClass(); Saved.Level = Effect->Spec.GetLevel(); Saved.Stacks = Effect->Spec.GetStackCount();
		Saved.TagMagnitudes = Effect->Spec.SetByCallerTagMagnitudes; Saved.NameMagnitudes = Effect->Spec.SetByCallerNameMagnitudes;
		Saved.DynamicAssetTags = Effect->Spec.GetDynamicAssetTags(); Saved.DynamicGrantedTags = Effect->Spec.DynamicGrantedTags;
		Saved.OwnerComponent = MassEffectOwner(NPC, *Effect);
		Saved.OwnerItem = MassItemOwner(NPC, Effect->Spec.GetContext().GetSourceObject());
		const auto* SourceObject = Effect->Spec.GetContext().GetSourceObject();
		if (SourceObject && SourceObject != NPC && Saved.OwnerComponent.IsNone() && !Saved.OwnerItem.IsValid())
		{ Error = TEXT("A persistent GAS effect source has no stable component/item ownership."); return false; }
		for (int32 I = 0; I < Effect->Spec.Modifiers.Num(); ++I) { Saved.ModifierMagnitudes.Add(Effect->Spec.GetModifierMagnitude(I)); }
	}
	TArray<FGameplayAttribute> Attributes; ASC->GetAllAttributes(Attributes);
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		const float Base = ASC->GetNumericAttributeBase(Attribute);
		if (!FMath::IsFinite(Base) || Result.AttributeBases.Contains(Attribute.GetName()))
		{ Error = TEXT("GAS attribute bases must be finite and unambiguous."); return false; }
		Result.AttributeBases.Add(Attribute.GetName(), Base);
	}
	TArray<AActor*> VisualActors; NPC->GetAllChildActors(VisualActors); VisualActors.Add(NPC);
	for (int32 Index = 0; Index < VisualActors.Num(); ++Index)
	{
		TArray<AActor*> Attached; VisualActors[Index]->GetAttachedActors(Attached);
		for (auto* Actor : Attached) { VisualActors.AddUnique(Actor); }
	}
	for (AActor* VisualActor : VisualActors)
	{
		if (!IsValid(VisualActor) || VisualActor->IsHidden()) { continue; }
		TInlineComponentArray<UStaticMeshComponent*> Statics(VisualActor);
		for (auto* Mesh : Statics)
		{
			if (!Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->GetStaticMesh()) { continue; }
			if (Mesh->IsSimulatingPhysics()) { Error = TEXT("An equipped visual is simulating physics; conversion requires settled visuals."); return false; }
			auto& Saved = Result.Meshes.AddDefaulted_GetRef(); Saved.StaticMesh = Mesh->GetStaticMesh();
			Saved.RelativeTransform = Mesh->GetComponentTransform().GetRelativeTransform(NPC->GetActorTransform());
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
			{
				if (Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I))) { Error = TEXT("Dynamic material overrides require an asset-backed presentation profile before conversion."); return false; }
				Saved.Materials.Add(Mesh->GetMaterial(I));
			}
		}
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(VisualActor);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->GetSkeletalMeshAsset()) { continue; }
			if (Mesh->IsSimulatingPhysics()) { Error = TEXT("Physics/ragdoll poses cannot convert during simulation."); return false; }
			auto& Saved = Result.Meshes.AddDefaulted_GetRef(); Saved.Mesh = Mesh->GetSkeletalMeshAsset(); Saved.ComponentName = Mesh->GetFName();
			if (const auto* Profile = MassAnimationProfiles.Find(FindParticipantId(NPC)))
			{
				Saved.RouteAnimation = Profile->ComponentAnimations.FindRef(Saved.ComponentName);
				Saved.AnimationReferenceSpeed = Profile->ReferenceSpeed;
			}
			Saved.bRequiresSnapshotBlend = Mesh->GetAnimInstance() != nullptr;
			if (Saved.bRequiresSnapshotBlend && !Cast<UNarrativeAnimInstance>(Mesh->GetAnimInstance()))
			{ Error = TEXT("Animated visual must support Narrative's existing pose-snapshot blend contract."); return false; }
			Saved.RelativeTransform = Mesh->GetComponentTransform().GetRelativeTransform(NPC->GetActorTransform());
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
			{
				if (Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I))) { Error = TEXT("Dynamic material overrides require an asset-backed presentation profile before conversion."); return false; }
				Saved.Materials.Add(Mesh->GetMaterial(I));
			}
			const int32 Count = Mesh->GetNumBones();
			for (int32 I = 0; I < Count; ++I)
			{
				const FName Bone = Mesh->GetBoneName(I); Saved.Bones.Add(Bone);
				Saved.ComponentSpacePose.Add(Mesh->GetSocketTransform(Bone, RTS_Component));
				const int32 ParentIndex = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(I);
				Saved.LocalPose.Add(ParentIndex == INDEX_NONE ? Saved.ComponentSpacePose.Last()
					: Saved.ComponentSpacePose.Last().GetRelativeTransform(Mesh->GetSocketTransform(Mesh->GetBoneName(ParentIndex), RTS_Component)));
				if (Mesh->IsBoneHiddenByName(Bone)) { Saved.HiddenBones.Add(Bone); }
			}
		}
	}
	if (Result.Meshes.IsEmpty() || Result.Meshes.Num() > 32)
	{ Error = TEXT("Campaign Mass requires one through 32 initialized visual parts; no pedestrian fallback is allowed."); return false; }
	Out = MoveTemp(Result); return true;
}

bool ASovEncounterDirector::EnsureMassAssets(const FSovEncounterMassRecord& Record, FString& Error)
{
	if (bEndingPlay || Record.NPC.ParticipantId.IsNone()) { Error = TEXT("Mass residency requires a live participant owner."); return false; }
	TArray<FSoftObjectPath> Paths;
	const auto AddPath = [&Paths](const FSoftObjectPath& Path) { if (!Path.IsNull()) { Paths.AddUnique(Path); } };
	AddPath(Record.NPC.Definition.ToSoftObjectPath()); AddPath(Record.NPC.ActorRecord.ActorSoftClass.ToSoftObjectPath());
	if (Record.Transfer.Meshes.Num() > 32) { Error = TEXT("Mass asset residency exceeds the visual-part limit."); return false; }
	for (const auto& Mesh : Record.Transfer.Meshes)
	{
		AddPath(Mesh.Mesh.ToSoftObjectPath()); AddPath(Mesh.StaticMesh.ToSoftObjectPath()); AddPath(Mesh.RouteAnimation.ToSoftObjectPath());
		if (Mesh.Materials.Num() > 64) { Error = TEXT("Mass visual exceeds the material-slot limit."); return false; }
		for (const auto& Material : Mesh.Materials) { AddPath(Material.ToSoftObjectPath()); }
	}
	if (Paths.Num() > 1024) { Error = TEXT("Mass asset residency exceeds the participant budget."); return false; }
	FSovCampaignMassAssetResidency Lease;
	bool bResident = true;
	for (const auto& Path : Paths)
	{
		UObject* Asset = Path.ResolveObject();
		if (!IsValid(Asset)) { bResident = false; } else { Lease.Assets.Add(Asset); }
	}
	const FName Id = Record.NPC.ParticipantId;
	if (bResident)
	{
		// Acquire strong ownership before releasing the asynchronous handle or original actor.
		MassAssetResidency.Add(Id, MoveTemp(Lease));
		MassAssetLoads.Remove(Id); MassAssetLoadDeadlines.Remove(Id);
		return true;
	}
	if (!MassAssetLoads.Contains(Id))
	{
		auto Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate());
		if (!Handle.IsValid()) { Error = TEXT("Mass assets could not be queued; the source actor/record is retained."); return false; }
		MassAssetLoads.Add(Id, MoveTemp(Handle));
		MassAssetLoadDeadlines.Add(Id, FPlatformTime::Seconds() + FMath::Clamp(RestoreTimeoutSeconds, 1.f, 120.f));
		SetActorTickEnabled(true);
	}
	Error = TEXT("Mass assets are loading asynchronously; retry the authored boundary while the source actor remains authoritative.");
	return false;
}

void ASovEncounterDirector::TickMassAssetLoads()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SovEncounter_MassAssetResidency);
	if (!HasAuthority() || bEndingPlay || bMutationInProgress || MassAssetLoads.IsEmpty()) { return; }
	const uint64 Generation = RestoreGeneration; const FGuid Attempt = AttemptId;
	const auto Current = [this, Generation, Attempt]()
	{ return IsValid(this) && !IsActorBeingDestroyed() && !bEndingPlay && RestoreGeneration == Generation && AttemptId == Attempt; };
	TArray<FName> Ids; MassAssetLoads.GetKeys(Ids);
	for (FName Id : Ids)
	{
		if (!Current()) { return; }
		const auto Handle = MassAssetLoads.FindRef(Id);
		const bool bTimedOut = FPlatformTime::Seconds() >= MassAssetLoadDeadlines.FindRef(Id);
		if (!Handle.IsValid() || bTimedOut || Handle->WasCanceled())
		{
			if (Handle.IsValid()) { Handle->CancelHandle(); }
			MassAssetLoads.Remove(Id); MassAssetLoadDeadlines.Remove(Id);
			if (PendingMassRestores.Remove(Id))
			{ OnEncounterRestoreFailed.Broadcast(TEXT("Mass asset loading failed or timed out; the saved record remains available for encounter retry.")); }
			continue;
		}
		if (!PendingMassRestores.Contains(Id) || !Handle->HasLoadCompleted()) { continue; }
		auto* Record = MassParticipants.FindByPredicate([Id](const auto& Value) { return Value.NPC.ParticipantId == Id; });
		FString Error;
		if (!Record || !EnsureMassAssets(*Record, Error))
		{
			Handle->CancelHandle(); MassAssetLoads.Remove(Id); MassAssetLoadDeadlines.Remove(Id); PendingMassRestores.Remove(Id);
			OnEncounterRestoreFailed.Broadcast(TEXT("A saved Mass asset is missing; encounter retry retains the checkpoint.")); continue;
		}
		PendingMassRestores.Remove(Id);
		if (!CreateMassEntity(*Record, Error)) { OnEncounterRestoreFailed.Broadcast(Error); }
	}
	if (Current() && MassAssetLoads.IsEmpty() && State != ESovEncounterState::Active && State != ESovEncounterState::Restoring)
	{ SetActorTickEnabled(false); }
}

bool ASovEncounterDirector::CreateMassEntity(FSovEncounterMassRecord& Record, FString& Error)
{
	if (Record.Tier == ESovCampaignRepresentationTier::Mass && !ASovCampaignMassProxy::ValidateAnimationProfile(Record.Transfer.Meshes, Error)) { return false; }
	auto* Spawner = GetWorld()->GetSubsystem<UMassSpawnerSubsystem>();
	auto* Entities = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!Spawner || !Entities || Entities->GetMutableEntityManager().IsProcessing())
	{ Error = TEXT("Mass subsystem unavailable or processing; retry at a game-thread boundary."); return false; }
	if (Record.NPC.ParticipantId.IsNone() || !Record.NPC.ActorRecord.ActorGUID.IsValid() || MassEntities.Contains(Record.NPC.ParticipantId)
		|| Record.NPC.ActorRecord.Transform.ContainsNaN() || !SovCampaignMassPolicy::ValidRoute(Record.Route.Num(), Record.RouteSpeed)
		|| Record.NextRoutePoint < 0 || Record.NextRoutePoint > Record.Route.Num())
	{ Error = TEXT("Saved Mass identity, transform or route is malformed or already owned."); return false; }
	for (const FVector Point : Record.Route) { if (Point.ContainsNaN()) { Error = TEXT("Saved Mass route contains a nonfinite point."); return false; } }
	if (MassConfig.IsEmpty())
	{
		MassConfig.SetOwner(*this); MassConfig.AddTrait(*NewObject<USovCampaignMassVisualizationTrait>(this));
	}
	const FMassEntityTemplate& Template = MassConfig.GetOrCreateEntityTemplate(*GetWorld());
	if (!Template.IsValid()) { Error = TEXT("Campaign Mass visualization template is invalid."); return false; }
	TArray<FMassEntityHandle> Spawned;
	// Retain creation context until identity and transform exist, before observers are dispatched.
	auto Creation = Spawner->SpawnEntities(Template, 1u, Spawned);
	if (Spawned.Num() != 1) { Error = TEXT("Mass did not create one participant entity."); return false; }
	FMassEntityManager& Manager = Entities->GetMutableEntityManager();
	const FMassEntityHandle Entity = Spawned[0];
	auto& Identity = Manager.GetFragmentDataChecked<FNarrativeMassParticipantFragment>(Entity);
	Identity.Owner = this; Identity.ActorIdentity = Record.NPC.ActorRecord.ActorGUID;
	Identity.ParticipantId = Record.NPC.ParticipantId; Identity.EncounterId = EncounterId;
	Identity.Generation = ++NextMassEpoch; Identity.bPresentationOnly = true;
	Manager.GetFragmentDataChecked<FTransformFragment>(Entity).GetMutableTransform() = Record.NPC.ActorRecord.Transform;
	auto& Route = Manager.GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity);
	Route.Points = Record.Route; Route.NextPoint = Record.NextRoutePoint; Route.Speed = Record.RouteSpeed;
	Route.bPresentationOnly = Record.Tier == ESovCampaignRepresentationTier::Presentation || State != ESovEncounterState::Active;
	MassEntities.Add(Identity.ParticipantId, Entity); MassEpochs.Add(Identity.ParticipantId, Identity.Generation);
	return true;
}

bool ASovEncounterDirector::SetParticipantRepresentation(FName Id, ESovCampaignRepresentationTier Tier, FName Boundary, FString& Error)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SovEncounter_SetParticipantRepresentation);
	Error.Reset();
	if (bEndingPlay || IsActorBeingDestroyed()) { Error = TEXT("Encounter teardown owns its participants."); return false; }
	FSovEncounterParticipant* Participant = Participants.FindByPredicate([Id](const auto& P) { return P.ParticipantId == Id; });
	if (!SovCampaignMassPolicy::CanTransition(static_cast<unsigned>(State), HasAuthority(), RepresentationBoundaries.Contains(Boundary),
		Participant && Participant->bAllowMassRepresentation, bMutationInProgress, !MassPromotions.IsEmpty(), DefeatedParticipants.Contains(Id), static_cast<unsigned>(Tier)))
	{ Error = TEXT("Representation transition requires an opted-in living participant, active encounter, named boundary, and no overlapping mutation/promotion."); return false; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	auto* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem || EntitySubsystem->GetMutableEntityManager().IsProcessing())
	{ Error = TEXT("Conversion must occur outside Mass processing."); return false; }
	FSovEncounterMassRecord* Existing = MassParticipants.FindByPredicate([Id](const auto& R) { return R.NPC.ParticipantId == Id; });
	if (Tier != ESovCampaignRepresentationTier::Actor)
	{
		if (Existing)
		{
			if (!EnsureMassAssets(*Existing, Error) || (Tier == ESovCampaignRepresentationTier::Mass
				&& !ASovCampaignMassProxy::ValidateAnimationProfile(Existing->Transfer.Meshes, Error))) { return false; }
			if (auto* Proxy = Cast<ASovCampaignMassProxy>(MassProxies.FindRef(Id).Get()))
			{
				Proxy->CaptureVisualState(Existing->Transfer.Meshes);
				if (!Proxy->RestoreVisuals(Existing->Transfer.Meshes, Tier == ESovCampaignRepresentationTier::Mass))
				{ Error = TEXT("The existing visual could not change tier; its prior identity is retained."); return false; }
			}
			Existing->Tier = Tier;
			if (auto* Entities = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
			{
				const FMassEntityHandle Entity = MassEntities.FindRef(Id);
				if (Entities->GetMutableEntityManager().IsEntityValid(Entity))
				{ Entities->GetMutableEntityManager().GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity).bPresentationOnly = Tier == ESovCampaignRepresentationTier::Presentation; }
			}
			return true;
		}
		ASovNPCCharacterBase* NPC = Participant->Character;
		if (!Coordination || !Coordination->CanChangeRepresentation(Id))
		{ Error = TEXT("A staged participant, attack reservation or unavailable coordinator prevents conversion."); return false; }
		NPC->SetEncounterOwned();
		FSovEncounterMassRecord Record; Record.Tier = Tier;
		if (!CaptureMassTransfer(NPC, Record.Transfer, Error) || !CaptureNPC(*Participant, Record.NPC, Error)) { return false; }
		// Keep the state after Narrative PrepareForSave callbacks update their visual/status caches.
		if (!CaptureMassTransfer(NPC, Record.Transfer, Error)) { return false; }
		if (!Record.NPC.Links.IsEmpty()) { Error = TEXT("Command-link participants require actor identities and cannot convert independently."); return false; }
		for (const auto& Other : Participants)
		{
			if (!IsValid(Other.Character)) { continue; }
			TInlineComponentArray<USovCommandLinkComponent*> Links(Other.Character);
			for (const auto* Link : Links)
			{ if (Link->GetCommandSource() == NPC || Link->GetLinkedActors().Contains(NPC)) { Error = TEXT("An incoming command link requires this actor."); return false; } }
		}
		if (GetParticipant(Id) != NPC || !IsValid(NPC) || !NPC->IsAlive()) { Error = TEXT("Participant changed during snapshot callbacks."); return false; }
		for (auto* Actor : AttemptActors)
		{
			TSet<const AActor*> Visited;
			for (const AActor* Owner = IsValid(Actor) ? Actor->GetOwner() : nullptr; Owner && !Visited.Contains(Owner); Owner = Owner->GetOwner())
			{
				if (Owner == NPC) { Error = TEXT("A live attempt actor still depends on this participant's combat ownership."); return false; }
				Visited.Add(Owner);
			}
		}
		// The owner record must exist before creation observers can request a representation.
		if (!EnsureMassAssets(Record, Error)) { return false; }
		if (Tier == ESovCampaignRepresentationTier::Mass && !ASovCampaignMassProxy::ValidateAnimationProfile(Record.Transfer.Meshes, Error))
		{ MassAssetResidency.Remove(Id); return false; }
		MassParticipants.Add(MoveTemp(Record));
		if (!CreateMassEntity(MassParticipants.Last(), Error))
		{ MassParticipants.RemoveAll([Id](const auto& R) { return R.NPC.ParticipantId == Id; }); MassAssetResidency.Remove(Id); return false; }
		// Commit null actor ownership before destruction callbacks; the same required participant is now the entity.
		Participant = Participants.FindByPredicate([Id](const auto& P) { return P.ParticipantId == Id; });
		Participant->Character = nullptr;
		Coordination->ReleaseRepresentationActor(Id, NPC);
		UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent();
		if (ASC) { ASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath); BoundDeathASCs.Remove(ASC); }
		AController* Controller = NPC->GetController();
		NPC->Destroy();
		if (IsValid(Controller) && (!Controller->GetPawn() || Controller->GetPawn() == NPC)) { Controller->Destroy(); }
		ForceNetUpdate(); return true;
	}
	if (!Existing) { return IsValid(Participant->Character); }
	if (!Coordination || !Coordination->CanPromoteRepresentation(Id))
	{ Error = TEXT("Promotion would exceed the registered A/B wave budget."); return false; }
	CaptureMassTransforms();
	const FSovEncounterNPCRecord Record = Existing->NPC;
	if (!EnsureMassAssets(*Existing, Error)) { return false; }
	UNPCDefinition* Definition = Record.Definition.Get();
	UClass* Class = Record.ActorRecord.ActorSoftClass.Get();
	FNPCSpawnInfo SpawnInfo = Record.SpawnInfo;
	FMemoryReader Reader(Record.SpawnInfoData); FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	FNPCSpawnInfo::StaticStruct()->SerializeItem(Archive, &SpawnInfo, nullptr); SpawnInfo.OwningSpawn.Reset();
	if (!Definition || !Class || Archive.IsError()) { Error = TEXT("Saved campaign definition/class/spawn overrides cannot promote."); return false; }
	auto* NPC = GetWorld()->SpawnActorDeferred<ASovNPCCharacterBase>(Class, Record.ActorRecord.Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(NPC)) { Error = TEXT("Campaign replacement could not spawn; Mass record retained."); return false; }
	MassPromotions.Add(Id, NPC); MassPromotionStarts.Add(Id, GetWorld()->GetTimeSeconds());
	Participant = Participants.FindByPredicate([Id](const auto& P) { return P.ParticipantId == Id; });
	Participant->Character = NPC;
	NPC->PrepareForEncounterRestore(SpawnInfo, Record.ActorRecord.ActorGUID); NPC->SetNPCDefinition(Definition);
	NPC->SetActorHiddenInGame(true); NPC->SetActorEnableCollision(false);
	NPC->FinishSpawning(Record.ActorRecord.Transform);
	if (!IsValid(NPC) || GetParticipant(Id) != NPC) { Error = TEXT("Promotion callbacks replaced the participant; retry remains available."); return false; }
	NPC->EnsureEncounterController(); SuspendActor(NPC);
	// Freeze C processing immediately while its actor replacement initializes.
	if (auto* Entities = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		const FMassEntityHandle Entity = MassEntities.FindRef(Id);
		if (Entities->GetMutableEntityManager().IsEntityValid(Entity))
		{ Entities->GetMutableEntityManager().GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity).bPresentationOnly = true; }
	}
	SetActorTickEnabled(true); return true;
}

bool ASovEncounterDirector::SetParticipantMassRoute(FName Id, const TArray<FVector>& Points, float Speed, FString& Error)
{
	Error.Reset();
	if (!HasAuthority() || bMutationInProgress || State != ESovEncounterState::Active || !MassPromotions.IsEmpty()
		|| !SovCampaignMassPolicy::ValidRoute(Points.Num(), Speed))
	{ Error = TEXT("Mass route needs an active unmutated encounter, at most 128 finite points, and speed from 0 through 1200 cm/s."); return false; }
	for (const FVector Point : Points) { if (Point.ContainsNaN()) { Error = TEXT("Route point is not finite."); return false; } }
	auto* Record = MassParticipants.FindByPredicate([Id](const auto& R) { return R.NPC.ParticipantId == Id; });
	auto* Entities = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	const FMassEntityHandle Entity = MassEntities.FindRef(Id);
	if (!Record || !Entities || Entities->GetMutableEntityManager().IsProcessing() || !Entities->GetMutableEntityManager().IsEntityValid(Entity))
	{ Error = TEXT("Participant has no current campaign Mass entity or Mass is processing."); return false; }
	if (Speed > 0.f && Record->Tier == ESovCampaignRepresentationTier::Mass
		&& !ASovCampaignMassProxy::ValidateAnimationProfile(Record->Transfer.Meshes, Error)) { return false; }
	Record->Route = Points; Record->RouteSpeed = Speed; Record->NextRoutePoint = 0;
	auto& Route = Entities->GetMutableEntityManager().GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity);
	Route.Points = Points; Route.Speed = Speed; Route.NextPoint = 0; return true;
}

bool ASovEncounterDirector::AcceptMassRepresentation(FMassEntityManager& Manager, FMassEntityHandle Entity, AActor& Actor, const FNarrativeMassParticipantFragment& Identity)
{
	const auto* Record = MassParticipants.FindByPredicate([&](const auto& R) { return R.NPC.ParticipantId == Identity.ParticipantId; });
	auto* Proxy = Cast<ASovCampaignMassProxy>(&Actor);
	if (!Proxy || !Record || Identity.Owner.Get() != this || Identity.EncounterId != EncounterId
		|| Identity.ActorIdentity != Record->NPC.ActorRecord.ActorGUID || !Manager.IsEntityValid(Entity)
		|| MassEntities.FindRef(Identity.ParticipantId) != Entity || MassEpochs.FindRef(Identity.ParticipantId) != Identity.Generation
		|| !HasAuthority() || Actor.GetWorld() != GetWorld() || IsActorBeingDestroyed()) { return false; }
	// A second actor cannot silently take over the same live receipt.
	const auto Prior = MassProxies.FindRef(Identity.ParticipantId);
	if (Prior.IsValid() && Prior.Get() != &Actor) { return false; }
	MassProxies.Add(Identity.ParticipantId, &Actor); Actor.SetOwner(this);
	const bool bVisuals = Proxy->RestoreVisuals(Record->Transfer.Meshes, Record->Tier == ESovCampaignRepresentationTier::Mass);
	Proxy->SetRouteMotion(State == ESovEncounterState::Active && Record->Tier == ESovCampaignRepresentationTier::Mass
		&& Record->Route.IsValidIndex(Record->NextRoutePoint) && !MassPromotions.Contains(Identity.ParticipantId), Record->RouteSpeed);
	return bVisuals && IsMassRepresentationCurrent(Manager, Entity, Actor, Identity);
}

bool ASovEncounterDirector::IsMassRepresentationCurrent(FMassEntityManager& Manager, FMassEntityHandle Entity, const AActor& Actor, const FNarrativeMassParticipantFragment& Identity) const
{
	const auto* Record = MassParticipants.FindByPredicate([&](const auto& R) { return R.NPC.ParticipantId == Identity.ParticipantId; });
	return IsValid(this) && !IsActorBeingDestroyed() && HasAuthority() && IsValid(&Actor) && Actor.GetWorld() == GetWorld()
		&& MassProxies.FindRef(Identity.ParticipantId).Get() == &Actor && Record && Identity.EncounterId == EncounterId
		&& SovCampaignMassPolicy::SameReceipt(MassEpochs.FindRef(Identity.ParticipantId), Identity.Generation, Identity.Owner.Get() == this,
			Manager.IsEntityValid(Entity) && MassEntities.FindRef(Identity.ParticipantId) == Entity, Identity.ActorIdentity == Record->NPC.ActorRecord.ActorGUID);
}

void ASovEncounterDirector::ReleaseMassRepresentation(FMassEntityManager& Manager, FMassEntityHandle Entity, AActor& Actor, const FNarrativeMassParticipantFragment& Identity)
{
	if (MassEntities.FindRef(Identity.ParticipantId) == Entity && MassEpochs.FindRef(Identity.ParticipantId) == Identity.Generation
		&& MassProxies.FindRef(Identity.ParticipantId).Get() == &Actor) { MassProxies.Remove(Identity.ParticipantId); }
}

void ASovEncounterDirector::CaptureMassTransforms()
{
	auto* Entities = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Entities) { return; }
	for (auto& Record : MassParticipants)
	{
		const FMassEntityHandle Entity = MassEntities.FindRef(Record.NPC.ParticipantId);
		FMassEntityManager& Manager = Entities->GetMutableEntityManager();
		if (!Manager.IsEntityValid(Entity)) { continue; }
		Record.NPC.ActorRecord.Transform = Manager.GetFragmentDataChecked<FTransformFragment>(Entity).GetTransform();
		Record.NextRoutePoint = Manager.GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity).NextPoint;
		if (auto* Proxy = Cast<ASovCampaignMassProxy>(MassProxies.FindRef(Record.NPC.ParticipantId).Get()))
		{ Proxy->CaptureVisualState(Record.Transfer.Meshes); }
	}
}

void ASovEncounterDirector::RefreshMassProcessingState()
{
	auto* Entities = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Entities) { return; }
	FMassEntityManager& Manager = Entities->GetMutableEntityManager();
	for (const auto& Record : MassParticipants)
	{
		const FName Id = Record.NPC.ParticipantId;
		const FMassEntityHandle Entity = MassEntities.FindRef(Id);
		if (!Manager.IsEntityValid(Entity)) { continue; }
		Manager.GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity).bPresentationOnly =
			State != ESovEncounterState::Active || Record.Tier == ESovCampaignRepresentationTier::Presentation || MassPromotions.Contains(Id);
		if (auto* Proxy = Cast<ASovCampaignMassProxy>(MassProxies.FindRef(Id).Get()))
		{ Proxy->SetRouteMotion(State == ESovEncounterState::Active && Record.Tier == ESovCampaignRepresentationTier::Mass
			&& Record.Route.IsValidIndex(Record.NextRoutePoint) && !MassPromotions.Contains(Id), Record.RouteSpeed); }
	}
}

void ASovEncounterDirector::DestroyMassEntity(FName Id)
{
	FMassEntityHandle Entity; MassEntities.RemoveAndCopyValue(Id, Entity); MassEpochs.Remove(Id);
	TWeakObjectPtr<AActor> Proxy; MassProxies.RemoveAndCopyValue(Id, Proxy);
	if (auto* Entities = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr)
	{
		FMassEntityManager& Manager = Entities->GetMutableEntityManager();
		if (Manager.IsEntityValid(Entity))
		{
			auto& Identity = Manager.GetFragmentDataChecked<FNarrativeMassParticipantFragment>(Entity);
			Identity.Owner.Reset(); Identity.Generation = 0;
			// The representation fragment is authoritative even when owner acceptance rejected the new actor.
			if (auto* ActorFragment = Manager.GetFragmentDataPtr<FMassActorFragment>(Entity))
			{ if (ActorFragment->GetMutable()) { Proxy = ActorFragment->GetMutable(); } }
			if (Proxy.IsValid())
			{ if (auto* Receipt = Proxy->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>()) { Receipt->Reset(); } }
			if (auto* Representation = GetWorld()->GetSubsystem<UMassPedRepresentationSubsystem>())
			{
				FMassRepresentationFragment Visual = Manager.GetFragmentDataChecked<FMassRepresentationFragment>(Entity);
				bool bReleased = Representation->ReleaseTemplateActorOrCancelSpawning(Entity, Visual.HighResTemplateActorIndex, Proxy.Get(), Visual.ActorSpawnRequestHandle, true);
				if (!bReleased && Visual.LowResTemplateActorIndex != Visual.HighResTemplateActorIndex)
				{ bReleased = Representation->ReleaseTemplateActorOrCancelSpawning(Entity, Visual.LowResTemplateActorIndex, Proxy.Get(), Visual.ActorSpawnRequestHandle, true); }
				if (Manager.IsEntityValid(Entity))
				{
					if (auto* ActorFragment = Manager.GetFragmentDataPtr<FMassActorFragment>(Entity))
					{ ActorFragment->ResetAndUpdateHandleMap(GetWorld()->GetSubsystem<UMassActorSubsystem>()); }
				}
				if (bReleased) { Proxy.Reset(); }
			}
			if (Manager.IsEntityValid(Entity)) { Manager.Defer().DestroyEntity(Entity); }
		}
	}
	if (Proxy.IsValid())
	{
		if (auto* Receipt = Proxy->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>()) { Receipt->Reset(); }
		Proxy->Destroy();
	}
}

void ASovEncounterDirector::ClearMassRepresentations(bool bDiscardRecords)
{
	for (auto& Pair : MassAssetLoads) { if (Pair.Value.IsValid()) { Pair.Value->CancelHandle(); } }
	MassAssetLoads.Reset(); MassAssetLoadDeadlines.Reset(); PendingMassRestores.Reset(); MassAssetResidency.Reset();
	TArray<FName> Ids; MassEntities.GetKeys(Ids);
	for (FName Id : Ids) { DestroyMassEntity(Id); }
	const auto Pending = MoveTemp(MassPromotions); MassPromotions.Reset(); MassPromotionStarts.Reset();
	for (const auto& Pair : Pending)
	{
		ASovNPCCharacterBase* NPC = Pair.Value;
		if (!IsValid(NPC) || GetParticipant(Pair.Key) != NPC) { continue; }
		AController* Controller = NPC->GetController(); NPC->Destroy();
		if (IsValid(Controller) && (!Controller->GetPawn() || Controller->GetPawn() == NPC)) { Controller->Destroy(); }
	}
	if (bDiscardRecords) { MassParticipants.Reset(); }
}

bool ASovEncounterDirector::RestoreMassTransfer(ASovNPCCharacterBase* NPC, const FSovEncounterMassRecord& Record, FString& Error)
{
	const FName Id = Record.NPC.ParticipantId;
	const uint64 Epoch = MassEpochs.FindRef(Id);
	auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
	auto* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	const auto Current = [&]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Active && Epoch != 0
			&& MassEpochs.FindRef(Id) == Epoch && IsValid(NPC) && !NPC->IsActorBeingDestroyed()
			&& GetParticipant(Id) == NPC && MassPromotions.FindRef(Id) == NPC && IsValid(ASC) && ASC->GetAvatarActor() == NPC;
	};
	if (!Current() || !Save || !NPC->IsEncounterSnapshotReady()) { Error = TEXT("Promotion no longer owns a ready NPC/save subsystem."); return false; }
	FNarrativeActorRecord ActorRecord = Record.NPC.ActorRecord;
	ActorRecord.SavedComponents.RemoveAll([ASC](const auto& Component) { return Component.ComponentName == ASC->GetFName(); });
	NPC->SetWieldState(FWeaponWieldState());
	if (!Current()) { return false; }
	if (!Save->LoadActorFromRecord(NPC, ActorRecord) || !Current()) { Error = TEXT("Narrative could not restore campaign inventory/component state."); return false; }
	NPC->SetEncounterOwned(); // The director remains the only dynamic spawn owner, including older records.
	NPC->SetActorHiddenInGame(true); NPC->SetActorEnableCollision(false);
	if (!Current()) { return false; }
	NPC->SetActorTransform(Record.NPC.ActorRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (!Current()) { return false; }
	const FGameplayTagContainer PriorFactions = NPC->GetFactions();
	INarrativeTeamAgentInterface* Team = Cast<INarrativeTeamAgentInterface>(NPC);
	if (!Team) { Error = TEXT("Restored NPC has no Narrative faction authority."); return false; }
	for (FGameplayTag Tag : PriorFactions) { if (!Record.Transfer.Factions.HasTagExact(Tag)) { Team->RemoveFaction(Tag); if (!Current()) { return false; } } }
	for (FGameplayTag Tag : Record.Transfer.Factions) { Team->AddFaction(Tag); if (!Current()) { return false; } }
	FWeaponWieldState Wields; Wields.EquipSlots = Record.NPC.WieldEquipSlots; Wields.WieldSlots = Record.NPC.WieldSlots;
	NPC->SetWieldState(Wields);
	if (!Current()) { return false; }
	// Rebuild only the supported infinite self-authored effect subset. Captured contexts never retain old actors.
	const auto ExistingEffects = ASC->GetActiveEffects(FGameplayEffectQuery());
	for (FActiveGameplayEffectHandle Handle : ExistingEffects)
	{
		const auto* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (Effect && MassEffectOwner(NPC, *Effect).IsNone() && !MassItemOwner(NPC, Effect->Spec.GetContext().GetSourceObject()).IsValid())
		{ ASC->RemoveActiveGameplayEffect(Handle); }
		if (!Current()) { return false; }
	}
	for (const auto& Effect : Record.Transfer.Effects)
	{
		if (!Effect.OwnerComponent.IsNone() || Effect.OwnerItem.IsValid()) { continue; } // Preserve the component/item-owned removal handle.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext(); Context.AddSourceObject(NPC);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Effect.Class, Effect.Level, Context);
		if (!Spec.IsValid()) { Error = TEXT("A saved permanent self effect class cannot be reconstructed."); return false; }
		Spec.Data->SetStackCount(Effect.Stacks); Spec.Data->SetByCallerTagMagnitudes = Effect.TagMagnitudes;
		Spec.Data->SetByCallerNameMagnitudes = Effect.NameMagnitudes;
		Spec.Data->AppendDynamicAssetTags(Effect.DynamicAssetTags); Spec.Data->DynamicGrantedTags = Effect.DynamicGrantedTags;
		if (!ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).IsValid() || !Current())
		{ Error = TEXT("A saved permanent self effect failed to restore."); return false; }
	}
	TArray<FGameplayAbilitySpecHandle> MatchedGrants;
	for (const auto& Grant : Record.Transfer.Abilities)
	{
		FGameplayAbilitySpecHandle Handle;
		for (const auto& Existing : ASC->GetActivatableAbilities())
		{
			if (Existing.Ability && Existing.Ability->GetClass() == Grant.Class && Existing.Level == Grant.Level
				&& Existing.InputID == Grant.InputID && MassItemOwner(NPC, Existing.SourceObject.Get()) == Grant.OwnerItem
				&& !MatchedGrants.Contains(Existing.Handle)) { Handle = Existing.Handle; break; }
		}
		if (!Handle.IsValid())
		{
			if (Grant.OwnerItem.IsValid()) { Error = TEXT("Restored equipment did not recreate its owned GAS grant; promotion remains frozen."); return false; }
			FGameplayAbilitySpec GrantSpec(Grant.Class, Grant.Level, Grant.InputID, NPC); GrantSpec.GetDynamicSpecSourceTags() = Grant.DynamicTags;
			Handle = ASC->GiveAbility(GrantSpec);
		}
		if (!Current() || !Handle.IsValid()) { Error = TEXT("A campaign GAS grant could not rebind."); return false; }
		if (auto* GrantSpec = ASC->FindAbilitySpecFromHandle(Handle))
		{ GrantSpec->GetDynamicSpecSourceTags() = Grant.DynamicTags; ASC->MarkAbilitySpecDirty(*GrantSpec); }
		MatchedGrants.Add(Handle);
	}
	TArray<FGameplayAbilitySpecHandle> Excess;
	for (const auto& Existing : ASC->GetActivatableAbilities()) { if (!MatchedGrants.Contains(Existing.Handle)) { Excess.Add(Existing.Handle); } }
	for (auto Handle : Excess) { ASC->ClearAbility(Handle); if (!Current()) { return false; } }
	TArray<FGameplayAttribute> Attributes; ASC->GetAllAttributes(Attributes);
	for (const auto& Attribute : Attributes)
	{
		const float* Value = Record.Transfer.AttributeBases.Find(Attribute.GetName());
		if (!Value || !FMath::IsFinite(*Value)) { Error = TEXT("GAS attribute schema changed during promotion."); return false; }
		ASC->SetNumericAttributeBase(Attribute, *Value);
		if (!Current()) { return false; }
	}
	if (!USovEncounterSnapshotLibrary::RestoreResources(ASC, Record.NPC.Resources) || !Current()) { Error = TEXT("Campaign resource currents failed to restore."); return false; }
	auto* Weak = NPC->FindComponentByClass<USovWeakPointComponent>();
	auto* Sever = NPC->FindComponentByClass<USovDismembermentComponent>();
	if (Record.NPC.bHasWeakPoints && (!Weak || !Weak->RestoreWeakPointState(Record.NPC.WeakPoints)))
	{ Error = TEXT("Saved weak-point schema is incompatible."); return false; }
	if (!Current()) { return false; }
	if (Record.NPC.SeveredRegionMask != 0 && (!Sever || !Sever->RestoreSeveredRegionMask(Record.NPC.SeveredRegionMask)))
	{ Error = TEXT("Saved sever/injury state is incompatible."); return false; }
	if (!Current()) { return false; }
	TArray<AActor*> VisualActors; NPC->GetAllChildActors(VisualActors); VisualActors.Add(NPC);
	for (int32 Index = 0; Index < VisualActors.Num(); ++Index)
	{
		TArray<AActor*> Attached; VisualActors[Index]->GetAttachedActors(Attached);
		for (auto* Actor : Attached) { VisualActors.AddUnique(Actor); }
	}
	TSet<const USkeletalMeshComponent*> RestoredMeshes;
	for (const auto& SavedMesh : Record.Transfer.Meshes)
	{
		if (!SavedMesh.bRequiresSnapshotBlend) { continue; }
		USkeletalMeshComponent* TargetMesh = nullptr;
		for (auto* VisualActor : VisualActors)
		{
			TInlineComponentArray<USkeletalMeshComponent*> Meshes(VisualActor);
			for (auto* Mesh : Meshes)
			{
				if (Mesh->GetFName() == SavedMesh.ComponentName && Mesh->GetSkeletalMeshAsset() == SavedMesh.Mesh.Get()
					&& !RestoredMeshes.Contains(Mesh)) { TargetMesh = Mesh; break; }
			}
			if (TargetMesh) { break; }
		}
		auto* Animation = TargetMesh ? Cast<UNarrativeAnimInstance>(TargetMesh->GetAnimInstance()) : nullptr;
		FPoseSnapshot Pose; Pose.bIsValid = true; Pose.BoneNames = SavedMesh.Bones; Pose.LocalTransforms = SavedMesh.LocalPose;
		Pose.SkeletalMeshName = SavedMesh.Mesh.IsValid() ? SavedMesh.Mesh->GetFName() : NAME_None;
		if (!Animation || !Animation->BlendFromRepresentationPose(Pose))
		{ Error = TEXT("Saved animation skeleton/pose blend contract is incompatible; promotion remains frozen."); return false; }
		RestoredMeshes.Add(TargetMesh);
		if (!Current()) { return false; }
	}
	for (const auto& Pair : Record.Transfer.TagCounts)
	{
		const int32 Count = ASC->GetTagCount(Pair.Key);
		if (Count < Pair.Value) { ASC->AddLooseGameplayTag(Pair.Key, Pair.Value - Count, EGameplayTagReplicationState::TagAndCountToAll); }
		if (!Current()) { return false; }
	}
	// Unknown extra status is not silently accepted. Director suspension contributions are accounted separately.
	FGameplayTagContainer RestoredTags; ASC->GetOwnedGameplayTags(RestoredTags);
	for (FGameplayTag Tag : RestoredTags)
	{
		int32 Owned = 0;
		if (Tag == FNarrativeGameplayTags::Get().State_Busy && OwnedBusySuspensions.Contains(ASC)) { ++Owned; }
		if (Tag == FNarrativeGameplayTags::Get().State_Invulnerable && OwnedProtectionSuspensions.Contains(ASC)) { ++Owned; }
		if (ASC->GetTagCount(Tag) - Owned != Record.Transfer.TagCounts.FindRef(Tag))
		{ Error = TEXT("GAS status counts differ after promotion; the replacement remains frozen."); return false; }
	}
	TArray<FActiveGameplayEffectHandle> VerifiedEffects;
	const auto CurrentEffects = ASC->GetActiveEffects(FGameplayEffectQuery());
	for (const auto& Saved : Record.Transfer.Effects)
	{
		bool bMatched = false;
		for (auto Handle : CurrentEffects)
		{
			const auto* Effect = ASC->GetActiveGameplayEffect(Handle);
			if (!Effect || VerifiedEffects.Contains(Handle) || Effect->Spec.Def->GetClass() != Saved.Class
				|| Effect->Spec.GetLevel() != Saved.Level || Effect->Spec.GetStackCount() != Saved.Stacks
				|| MassEffectOwner(NPC, *Effect) != Saved.OwnerComponent || Effect->Spec.Modifiers.Num() != Saved.ModifierMagnitudes.Num()
				|| MassItemOwner(NPC, Effect->Spec.GetContext().GetSourceObject()) != Saved.OwnerItem
				|| Effect->Spec.GetDynamicAssetTags() != Saved.DynamicAssetTags || Effect->Spec.DynamicGrantedTags != Saved.DynamicGrantedTags) { continue; }
			bool bMagnitudes = true;
			for (int32 I = 0; I < Saved.ModifierMagnitudes.Num(); ++I)
			{ bMagnitudes &= FMath::IsNearlyEqual(Effect->Spec.GetModifierMagnitude(I), Saved.ModifierMagnitudes[I], 0.001f); }
			if (bMagnitudes) { VerifiedEffects.Add(Handle); bMatched = true; break; }
		}
		if (!bMatched) { Error = TEXT("Permanent GAS status/owner/magnitude changed during reconstruction; replacement remains frozen."); return false; }
	}
	if (VerifiedEffects.Num() != CurrentEffects.Num()) { Error = TEXT("Unexpected GAS effect appeared during promotion."); return false; }
	Save->RefreshStableActorIdentity(NPC);
	if (!Current()) { return false; }
	if (auto* Characters = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>()) { Characters->RegisterCharacter(NPC); }
	return Current();
}

void ASovEncounterDirector::TickMassPromotions()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SovEncounter_MassPromotion);
	if (!HasAuthority() || bMutationInProgress || MassPromotions.IsEmpty()) { return; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	const auto Pending = MassPromotions;
	for (const auto& Pair : Pending)
	{
		const FName Id = Pair.Key; ASovNPCCharacterBase* NPC = Pair.Value;
		if (State != ESovEncounterState::Active) { return; } // Retry owns cleanup in Failed/Restoring.
		const auto* Found = MassParticipants.FindByPredicate([Id](const auto& R) { return R.NPC.ParticipantId == Id; });
		FString Error;
		if (!Found || !IsValid(NPC) || GetParticipant(Id) != NPC) { Error = TEXT("Campaign promotion lost its exact participant identity."); }
		else if (!NPC->IsEncounterSnapshotReady())
		{
			if (GetWorld()->GetTimeSeconds() - MassPromotionStarts.FindRef(Id) < FMath::Clamp(RestoreTimeoutSeconds, 1.f, 120.f)) { continue; }
			Error = TEXT("Campaign promotion timed out initializing the saved NPC definition/appearance.");
		}
		else
		{
			const FSovEncounterMassRecord Record = *Found;
			SuspendActor(NPC);
			if (!IsValid(NPC) || GetParticipant(Id) != NPC || State != ESovEncounterState::Active) { return; }
			if (RestoreMassTransfer(NPC, Record, Error))
			{
				const uint64 Epoch = MassEpochs.FindRef(Id);
				UNarrativeAbilitySystemComponent* const PromotedASC = NPC->GetNarrativeAbilitySystemComponent();
				AController* const PromotedController = NPC->GetController();
				const FGuid PromotedAttempt = AttemptId;
				const auto Current = [&]() { return IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Active
					&& IsValid(NPC) && !NPC->IsActorBeingDestroyed() && NPC->IsAlive() && GetParticipant(Id) == NPC && MassEpochs.FindRef(Id) == Epoch
					&& AttemptId == PromotedAttempt && IsValid(PromotedASC) && NPC->GetNarrativeAbilitySystemComponent() == PromotedASC
					&& PromotedASC->GetAvatarActor() == NPC && NPC->GetController() == PromotedController
					&& (!PromotedController || (IsValid(PromotedController) && PromotedController->GetPawn() == NPC)); };
				if (!Current()) { return; }
				if (auto* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
				{
					if (!Save->SaveSingleActor(NPC) || !Current())
					{ SetState(ESovEncounterState::Failed); OnEncounterRestoreFailed.Broadcast(TEXT("Promoted actor could not replace its prior world record.")); return; }
				}
				// The replacement needs admission authority before Busy-tag callbacks can run.
				if (!NPC->GetNarrativeAbilitySystemComponent()->SetBotAttackCoordinator(Coordination))
				{ SetState(ESovEncounterState::Failed); OnEncounterRestoreFailed.Broadcast(TEXT("Promotion could not bind encounter attack authority.")); return; }
				// Release only this replacement's own director lease, never entry/future-wave leases on other actors.
				if (!ReleaseActorSuspension(NPC, Current) || !Current())
				{ if (IsValid(this) && !IsActorBeingDestroyed() && AttemptId == PromotedAttempt) { SetState(ESovEncounterState::Failed); } return; }
				NPC->SetActorHiddenInGame(false);
				if (!Current()) { return; }
				NPC->SetActorEnableCollision(true);
				if (!Current()) { return; }
				DestroyMassEntity(Id);
				if (!IsValid(this) || IsActorBeingDestroyed() || AttemptId != PromotedAttempt || !IsValid(NPC)
					|| GetParticipant(Id) != NPC || NPC->GetNarrativeAbilitySystemComponent() != PromotedASC || PromotedASC->GetAvatarActor() != NPC) { return; }
				MassParticipants.RemoveAll([Id](const auto& R) { return R.NPC.ParticipantId == Id; });
				MassPromotions.Remove(Id); MassPromotionStarts.Remove(Id);
				MassAssetResidency.Remove(Id);
				BindDeaths();
				Coordination->RefreshRepresentationBindings(); ForceNetUpdate();
				continue;
			}
		}
		if (Error.IsEmpty()) { Error = TEXT("Campaign promotion was superseded by a callback; retry is required."); }
		// Fail closed. Keep the saved C/D state and checkpoint for retry; never release a partially restored combatant.
		SetState(ESovEncounterState::Failed); OnEncounterRestoreFailed.Broadcast(Error); return;
	}
	// Active encounters retain reconciliation ticks even after the final promotion.
	if (MassPromotions.IsEmpty() && State != ESovEncounterState::Restoring && State != ESovEncounterState::Active) { SetActorTickEnabled(false); }
}
