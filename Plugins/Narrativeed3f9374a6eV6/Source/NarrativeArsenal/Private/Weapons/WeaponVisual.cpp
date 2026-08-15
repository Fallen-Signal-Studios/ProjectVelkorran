// Copyright Narrative Tools 2025.


#include "Weapons/WeaponVisual.h"

#include "KismetTraceUtils.h"
#include "NarrativeArsenal.h"
#include "Weapons/WeaponAnimPose.h"
#include "GameFramework/Character.h"
#include "Items/WeaponItem.h"
#include "Items/WeaponAttachmentItem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include <Animation/AnimMontage.h>
#include <Animation/Skeleton.h>
#include <Components/SkeletalMeshComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Animation/AnimSequence.h>
#include <Engine/SkeletalMesh.h>
#include <BonePose.h>
#include <BoneContainer.h>
#include <Net/UnrealNetwork.h>

#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "NarrativeLogChannels.h"
#include "Character/NarrativeCharacterVisual.h"
#include "UnrealFramework/NarrativeAnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponVisual, Log, All)


static TAutoConsoleVariable<int32> CDrawMeleeTraces(
	TEXT("n.gas.DrawMeleeTraces"),
	false,
	TEXT("Whether to draw melee traces 0=Off 1=Player 2=All"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CDrawMeleeAimAssist(
	TEXT("n.gas.DrawMeleeAimAssist"),
	false,
	TEXT("Whether to draw melee aim assist warp 0=Off 1=Player"),
	ECVF_Default);


bool FWeaponVisualAttachState::CanAttach() const
{
	return IsValid(WeaponOwner) && IsValid(VisualOwner) && EquippedSlot.IsValid();
}

// Sets default values
AWeaponVisual::AWeaponVisual(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	auto Settings = UArsenalStatics::GetNarrativeProSettings();
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>("WeaponMesh");
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToChannel(Settings->WeaponTraceChannel, ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WeaponMesh->SetCastShadow(true);
	WeaponMesh->SetCastHiddenShadow(true);
	WeaponMesh->bAlwaysCreatePhysicsState = true;
	WeaponMesh->SetReceivesDecals(false);

	SetRootComponent(WeaponMesh);

	LocalWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>("LocalWeaponMesh");
	LocalWeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	LocalWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LocalWeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	LocalWeaponMesh->SetCastShadow(false);
	LocalWeaponMesh->SetReceivesDecals(false);
	LocalWeaponMesh->SetupAttachment(WeaponMesh);

	//TODO need to work out why this isn't working in networked games - charvisual hides manually for now (possibly a bug in Epics FirstPerson rendering)
	//LocalWeaponMesh->SetOnlyOwnerSee(true);

	//Needed to stop attachment bugging out in network  - TODO should be able to fix this so doesn't matter 
	//LocalWeaponMesh->SetIsReplicated(true);
	//WeaponMesh->SetIsReplicated(true);

	bReplicates = true; 
	bNetUseOwnerRelevancy = true;

	bAttachedSuccesfully = false; 
}

class ANarrativeCharacter* AWeaponVisual::GetNarrativeCharacter() const
{
	if (INarrativeCharacterOwner* CharOwner = Cast<INarrativeCharacterOwner>(GetOwner()))
	{
		return CharOwner->GetNarrativeCharacter();
	}

	return nullptr; 
}

void AWeaponVisual::BeginPlay()
{
	Super::BeginPlay(); 

	//Need to spawn in all the weapons attachments too, and apply their mods - we'll simply just re-call AddAttachment for all attachments as it does this. 
	if (WeaponOwner)
	{	
		for (auto& AttachmentKVP : WeaponOwner->WeaponAttachments)
		{
			UWeaponAttachmentItem* Attachment = AttachmentKVP.Value;
			//fails as charowner not valid 
			if (Attachment)
			{
				Attachment->HandleAttach(WeaponOwner);
				WeaponOwner->AddAttachmentVisual(Attachment);
			}
		}
	}
	
	OnRep_AttachState();
	
	//UpdateWeaponAttachment();
}

void AWeaponVisual::Destroyed()
{
	Super::Destroyed();

	if (ANarrativeCharacterVisual* OurVisual = AttachState.VisualOwner)
	{
		if (OurVisual->SpawnedWeaponVisuals.Contains(AttachState.EquippedSlot))
		{
			//If our visual is at the slot still and hasn't been replaced, remove the visual so our SpawnedWeaponVisuals is synced up 
			if (OurVisual->SpawnedWeaponVisuals[AttachState.EquippedSlot] == this)
			{
				OurVisual->SpawnedWeaponVisuals.Remove(AttachState.EquippedSlot);		
			}
		}
	}

}

void AWeaponVisual::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AWeaponVisual, AttachState, COND_None, REPNOTIFY_Always);
	//DOREPLIFETIME_CONDITION(AWeaponVisual, VisualOwner, COND_InitialOnly);
	//DOREPLIFETIME_CONDITION(AWeaponVisual, WeaponOwner, COND_InitialOnly);
}

TSubclassOf<class UNarrativeAnimInstance> AWeaponVisual::GetWeaponOverlayLayer_Implementation(bool bFirstPersonMesh)
{
	if(CharacterOwner)
	{
		TArray<UWeaponItem*> Weapons = CharacterOwner->GetWieldedWeapons();

		//Grab 1P Anim layer, falling back to 3P if a 1P wasn't set. 
		if (bFirstPersonMesh)
		{
			if (Weapons.Num() == 1)
			{
				if (Weapon1PAnimLayer)
				{
					return Weapon1PAnimLayer;
				}
				else
				{
					return DefaultWeaponAnimLayer;
				}
			}
			else
			{
				if (DualWieldWeapon1PAnimLayer)
				{
					return DualWieldWeapon1PAnimLayer;
				}
				else
				{
					return DualWieldWeaponAnimLayer;
				}
			}
		}
		else
		{
			//For things like sword and shield, this logic won't be enough and is why we allow this function to be overriden. 
			if (Weapons.Num() == 1)
			{
				return DefaultWeaponAnimLayer;
			}
			else
			{
				return DualWieldWeaponAnimLayer;
			}
		}
	}

	return DefaultWeaponAnimLayer;
}

void AWeaponVisual::RegisterDefaultAttachment(const FGameplayTag& Slot, class UStaticMeshComponent* Mesh, class UStaticMeshComponent* LocalMesh)
{
	AttachmentMeshComps.Add(Slot, Mesh);
	LocalAttachmentMeshComps.Add(Slot, LocalMesh);

	if (UStaticMesh* MeshAsset = Mesh->GetStaticMesh())
	{
		AttachmentMeshDefaultMeshes.Add(Slot, MeshAsset);
	}
}

void AWeaponVisual::HandlePerspectiveUpdate_Implementation(const bool bIsFirstPerson)
{
	
	//Holstered weapons need hidden in first person, as sometimes they go in front of the camera and look jank. 
	bool bHolstered = !AttachState.WieldedSlot.IsValid();
	
	//Update the weapon meshes visibility and firstperson rendering flags. 
	if (WeaponMesh)
	{
		//WeaponMesh->SetVisibility(!bIsFirstPerson);
		WeaponMesh->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
	}

	for (auto& Attachment : AttachmentMeshComps)
	{
		if (UStaticMeshComponent* Attachment3P = Attachment.Value)
		{
			//Attachment3P->SetVisibility(!bIsFirstPerson);
			Attachment3P->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
		}
	}

	if (LocalWeaponMesh)
	{
		LocalWeaponMesh->SetVisibility(bIsFirstPerson && !bHolstered, true);
		LocalWeaponMesh->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	}

	for (auto& Attachment : LocalAttachmentMeshComps)
	{
		if (UStaticMeshComponent* Attachment1P = Attachment.Value)
		{
			Attachment1P->SetVisibility(bIsFirstPerson && !bHolstered, true);
			Attachment1P->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
		}
	}
}

void AWeaponVisual::OnWielded()
{
	BPHandleWield();
}

void AWeaponVisual::OnHolstered()
{
	BPHandleHolster();
}

void AWeaponVisual::HandleAttachedToOwner_Implementation()
{
	
}

void AWeaponVisual::HandleAddAttachment_Implementation(class UWeaponAttachmentItem* Attachment, const FWeaponAttachmentSlotConfig& WeaponSlotConfig)
{
	if (WeaponOwner && Attachment && Attachment->AttachmentMesh)
	{
		FGameplayTag Slot = Attachment->WeaponAttachmentSlot;

		if (!AttachmentMeshComps.Contains(Slot))
		{
			if (UStaticMeshComponent* NewMesh = Cast<UStaticMeshComponent>(AddComponentByClass(UStaticMeshComponent::StaticClass(), true, FTransform::Identity, false)))
			{
				NewMesh->SetCastHiddenShadow(true);
				NewMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
				NewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				AttachmentMeshComps.Add(Slot, NewMesh);
			}
		}

		if (LocalWeaponMesh && !LocalAttachmentMeshComps.Contains(Slot))
		{
			if (UStaticMeshComponent* NewMesh = Cast<UStaticMeshComponent>(AddComponentByClass(UStaticMeshComponent::StaticClass(), true, FTransform::Identity, false)))
			{
				NewMesh->SetCastShadow(false);
				NewMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
				NewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				LocalAttachmentMeshComps.Add(Slot, NewMesh);
			}
		}

		if (AttachmentMeshComps.Contains(Slot))
		{
			if (UStaticMeshComponent* AttachmentMesh = AttachmentMeshComps[Slot])
			{
				AttachmentMesh->EmptyOverrideMaterials();
				AttachmentMesh->SetStaticMesh(Attachment->AttachmentMesh);

				int32 i = 0;

				for (auto& MeshMat : Attachment->AttachmentMesh->GetStaticMaterials())
				{
					if (MeshMat.MaterialInterface)
					{
						AttachmentMesh->SetMaterial(i, MeshMat.MaterialInterface);
					}
					++i;
				}

				AttachmentMesh->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSlotConfig.SocketName);
			}
			else
			{
				UE_LOG(LogWeaponVisual, Error, TEXT("Unable to spawn mesh component for added attachment %s"), *GetNameSafe(Attachment));
			}
		}

		if (LocalWeaponMesh && LocalAttachmentMeshComps.Contains(Slot))
		{
			if (UStaticMeshComponent* AttachmentMesh = LocalAttachmentMeshComps[Slot])
			{
				AttachmentMesh->EmptyOverrideMaterials();
				AttachmentMesh->SetStaticMesh(Attachment->AttachmentMesh);

				int32 i = 0;

				for (auto& MeshMat : Attachment->AttachmentMesh->GetStaticMaterials())
				{
					if (MeshMat.MaterialInterface)
					{
						AttachmentMesh->SetMaterial(i, MeshMat.MaterialInterface);
					}
					++i;
				}

				AttachmentMesh->AttachToComponent(LocalWeaponMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSlotConfig.SocketName);
			}
			else
			{
				UE_LOG(LogWeaponVisual, Error, TEXT("Unable to spawn mesh component for added attachment %s"), *GetNameSafe(Attachment));
			}
		}
	}
}

void AWeaponVisual::HandleRemoveAttachment_Implementation(class UWeaponAttachmentItem* Attachment)
{
	FGameplayTag Slot = Attachment->WeaponAttachmentSlot;

	if (AttachmentMeshComps.Contains(Slot))
	{
		if (UStaticMeshComponent* AttachmentMesh = AttachmentMeshComps[Slot])
		{
			if (AttachmentMeshDefaultMeshes.Contains(Slot))
			{
				AttachmentMesh->EmptyOverrideMaterials();
				AttachmentMesh->SetStaticMesh(AttachmentMeshDefaultMeshes[Slot]);
				AttachmentMesh->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
				AttachmentMesh->SetRelativeTransform(FTransform());
			}
			else
			{
				AttachmentMesh->SetStaticMesh(nullptr);
			}
		}
		else
		{
			UE_LOG(LogWeaponVisual, Error, TEXT("Unable to spawn mesh component for added attachment %s"), *GetNameSafe(Attachment));
		}
	}

	if (LocalWeaponMesh && LocalAttachmentMeshComps.Contains(Slot))
	{
		if (UStaticMeshComponent* AttachmentMesh = LocalAttachmentMeshComps[Slot])
		{
			if (AttachmentMeshDefaultMeshes.Contains(Slot))
			{
				AttachmentMesh->EmptyOverrideMaterials();
				AttachmentMesh->SetStaticMesh(AttachmentMeshDefaultMeshes[Slot]);
				AttachmentMesh->AttachToComponent(LocalWeaponMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
				AttachmentMesh->SetRelativeTransform(FTransform());
			}
			else
			{
				AttachmentMesh->SetStaticMesh(nullptr);
			}
		}
		else
		{
			UE_LOG(LogWeaponVisual, Error, TEXT("Unable to spawn mesh component for added attachment %s"), *GetNameSafe(Attachment));
		}
	}
}

bool AWeaponVisual::SweepForHits(const FVector& Start, const FVector& End, const FQuat& Rot, const FVector& CapsuleSize, TArray<FHitResult>& OutHits)
{
	if (CharacterOwner)
	{
		FCollisionShape Shape;
		Shape.SetCapsule(FVector3f(CapsuleSize.X, CapsuleSize.Y, CapsuleSize.Z));

		FCollisionQueryParams CQP = CharacterOwner->GetIgnoreCharacterParams();

		CQP.AddIgnoredActors(CachedHitActors);

		CQP.bTraceComplex = true;
		CQP.bReturnPhysicalMaterial = true;


		int32 Draw = CDrawMeleeTraces.GetValueOnGameThread();

		bool bShouldDrawDebug = Draw > 0;

		if (Draw == 1)
		{
			if (CharacterOwner->IsBotControlled())
			{
				bShouldDrawDebug = false;
			}
		}

		if (bShouldDrawDebug)
		{
			CQP.TraceTag = FName("WeaponVisualSweep");

#if (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
			CQP.bDebugQuery = true;
			GetWorld()->DebugDrawTraceTag = true ? CQP.TraceTag : NAME_None;
#endif

		}

		if (GetWorld()->SweepMultiByChannel(OutHits, Start, End, Rot, TraceChannel_NarrativeWeapon, Shape, CQP))
		{
#if ENABLE_DRAW_DEBUG

				for (auto& Hit : OutHits)
				{
					if (AActor* Actor = Hit.GetActor())
					{
						CachedHitActors.Add(Actor);

						if (bShouldDrawDebug)
						{
							FString RoleStr = HasAuthority() ? "Server" : "Client";
							GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("%s: melee collider hit %s"), *RoleStr, *GetNameSafe(Actor)));
						}
					}
				}
#endif

			return true;
		}
	}

	return false; 
}

void AWeaponVisual::CacheAnimationTransform(
	const FAnimNotifyEventReference& AnimNotifyEventRef)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AWeaponVisual::CacheAnimationTransform)

	CurrentNotifyEvent = AnimNotifyEventRef;

	// We already cached this damage state data so there is no need to recalculate
	if (CachedDamageStateData.Contains(AnimNotifyEventRef.GetNotify()))
	{
		return;
	}

	TArray<FDamageStateData> DamageStateDatas;
	const auto NotifyEvent = AnimNotifyEventRef.GetNotify();
	
	USkeletalMeshComponent* OwnerMeshComponent = CharacterOwner->GetMesh();

	// Prepare socket and bone information along with montage references
	FTransform SocketTransform;
	int32 BoneIndex;
	auto NarrativeChar = Cast<ANarrativeCharacter>(CharacterOwner);
	const FName MeshBoneName = NarrativeChar->GetWeapon()->GetWeaponVisualAttachBone();

	// Fetch the socket info that we will be using
	OwnerMeshComponent->GetSocketInfoByName(MeshBoneName, SocketTransform, BoneIndex);
	const int32 MeshBoneIndex = OwnerMeshComponent->GetBoneIndex(MeshBoneName);

	// Get our current montage that is playing
	UAnimInstance* AnimInstance = OwnerMeshComponent->GetAnimInstance();
	FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveMontageInstance();
	const UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimNotifyEventRef.GetSourceObject());//MontageInstance->Montage;

	if (!AnimMontage || !MontageInstance)
	{
		return; 
	}

	// Gather animation data
	const FSlotAnimationTrack& AnimTrack = AnimMontage->SlotAnimTracks[0];
	const FAnimSegment* AnimSegment = AnimTrack.AnimTrack.GetSegmentAtTime(MontageInstance->GetPosition());
	const UAnimSequence* CurrentAnimSequence = Cast<UAnimSequence>(AnimSegment->GetAnimReference());
	USkeleton* Skeleton = CurrentAnimSequence->GetSkeleton();
	
	if (MeshBoneIndex == INDEX_NONE)
	{
		return;
	}

	const int32 SkeletonBoneIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(OwnerMeshComponent->GetSkeletalMeshAsset(), MeshBoneIndex);
	if (NotifyEvent && CurrentAnimSequence && Skeleton && SkeletonBoneIndex != INDEX_NONE)
	{
		// Gather time range of anim notify
		double StartTime = NotifyEvent->GetTriggerTime() + AnimSegment->AnimStartTime;
		double EndTime = StartTime + NotifyEvent->Duration;

		TArray<FBoneIndexType> RequiredBoneIndexArray;
		RequiredBoneIndexArray.AddUninitialized(Skeleton->GetReferenceSkeleton().GetNum());
		for (int RequiredBoneInd = 0; RequiredBoneInd < RequiredBoneIndexArray.Num(); RequiredBoneInd++)
		{
			if (RequiredBoneIndexArray.IsValidIndex(RequiredBoneInd))
			{
				RequiredBoneIndexArray[RequiredBoneInd] = StaticCast<FBoneIndexType>(RequiredBoneInd);
			}
		}
		
		FBoneContainer RequiredBones;
		RequiredBones.InitializeTo(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(), *Skeleton);
		RequiredBones.SetUseRAWData(false);
		RequiredBones.SetUseSourceData(false);
		RequiredBones.SetDisableRetargeting(false);

		// Init base pose data
		FCompactPose CompactPose;
		FBlendedCurve Curve;
		UE::Anim::FStackAttributeContainer Attributes;

		FAnimationPoseData PoseData(CompactPose, Curve, Attributes);
		FAnimExtractContext Context(0., true);

		FCompactPose BasePose;
		BasePose.SetBoneContainer(&RequiredBones);

		CompactPose.SetBoneContainer(&RequiredBones);
		Curve.InitFrom(RequiredBones);

		FWeaponAnimPose Pose;
		Pose.Init(RequiredBones);

		// start sampling animation and caching data
		const int32 SampleCount = GetDefault<UNarrativeCombatDeveloperSettings>()->MeleeCombatAnimSampleAmount;//30;
		for (int Index = 0; Index<SampleCount; Index++)
		{
			// Determine point in animation based on our sample count so sweeps are evenly distributed
			const double Alpha = StaticCast<double>(Index) / (StaticCast<double>(SampleCount) - 1);
			const double Time = FMath::Lerp(StartTime, EndTime, Alpha);

			FDamageStateData& DamageStateData = DamageStateDatas.Emplace_GetRef(FDamageStateData(Time - AnimSegment->AnimStartTime));
			Context.CurrentTime = Time;

			// Copy base pose as frame pose
			FWeaponAnimPose FramePose = Pose;

			// reinit curve
			Curve.InitFrom(RequiredBones);

			if (CurrentAnimSequence->IsValidAdditive())
			{
				CompactPose.ResetToAdditiveIdentity();
			}
			else
			{
				CompactPose.ResetToRefPose();
			}
			
			CurrentAnimSequence->GetAnimationPose(PoseData, Context);
			FramePose.SetPose(PoseData);

			const FTransform& RelativeTransform = FramePose.WorldSpacePoses[SkeletonBoneIndex];

			// Perform transform calculations to get socket position/rotation. Order is important here as scale should be applied at the end
			DamageStateData.RelativeLocation = RelativeTransform.GetLocation();
			DamageStateData.RelativeLocation += RelativeTransform.GetRotation().RotateVector(SocketTransform.GetLocation());
			DamageStateData.RelativeLocation *= OwnerMeshComponent->GetRelativeScale3D();
			DamageStateData.RelativeLocation *= CharacterOwner->GetActorRelativeScale3D();

			DamageStateData.RelativeRotation = RelativeTransform.GetRotation();
			DamageStateData.RelativeRotation *= SocketTransform.GetRotation();
		}
	}
	CachedDamageStateData.Add(AnimNotifyEventRef.GetNotify(), FDamageStateDataContainer(DamageStateDatas));
}

void AWeaponVisual::PerformCollisionCheck(TArray<FHitResult>& OutHits)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AWeaponVisual::PerformCollisionCheck)
	OutHits.Empty();
	
	USkeletalMeshComponent* OwnerMeshComponent = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	
	UAnimInstance* AnimInstance = OwnerMeshComponent ? OwnerMeshComponent->GetAnimInstance() : nullptr;
	FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveMontageInstance() : nullptr;

	// This could happen during gameplay when a montage has finished and is fading out. Therefore, this is expected behavior
	if (!MontageInstance)
	{
		return;
	}
	
	const double CurrentTime = MontageInstance->GetPosition();
	FQuat OwnerRotation = OwnerMeshComponent->GetComponentQuat();
	const FVector OwnerLocation = OwnerMeshComponent->GetBoneLocation(TEXT("root"));

	// If we decide to support look at rotation in the future, this would be a spot to hook it up
	FQuat LookAtRotation = FQuat::Identity;

	// Attempt to find cached attack data
	FDamageStateDataContainer* DamageStateData = CachedDamageStateData.Find(CurrentNotifyEvent.GetNotify());
	if (!DamageStateData)
	{
		UE_LOG(LogWeaponVisual, Error, TEXT("Unable to find cached damage state data for current notify event. No collision checks will be performed!"))
		return;
	}

	// Iterate through cached locations
	for (FDamageStateData& StateData : DamageStateData->DamageStateDatas)
	{
		if (CurrentTime < StateData.FrameTime || StateData.bTriggered)
		{
			continue;
		}

		for (FWeaponCollisionData& WeaponCollisionData : CollisionData)
		{
			FQuat Rotation = OwnerRotation;

			FQuat RelativeRotation = LookAtRotation * StateData.RelativeRotation;
			FVector CorrectedLocation = RelativeRotation.RotateVector(WeaponCollisionData.RelativeLocation);
			
			Rotation *= RelativeRotation;
			Rotation *= WeaponCollisionData.RelativeRotation;
			
			// correct last location if it was not initialized
			if (WeaponCollisionData.LastLocation.IsNearlyZero())
			{
				WeaponCollisionData.LastLocation = StateData.RelativeLocation + CorrectedLocation;
				WeaponCollisionData.LastLocation = LookAtRotation.RotateVector(WeaponCollisionData.LastLocation);
				WeaponCollisionData.LastLocation = OwnerRotation.RotateVector(WeaponCollisionData.LastLocation);
				WeaponCollisionData.LastLocation += OwnerLocation;
			}

			// Setup start/end locations of sweep
			FVector EndLocation = StateData.RelativeLocation + CorrectedLocation;
			EndLocation = LookAtRotation.RotateVector(EndLocation);
			EndLocation = OwnerRotation.RotateVector(EndLocation);
			EndLocation += OwnerLocation;

			FVector StartLocation = WeaponCollisionData.LastLocation;
			
			TArray<FHitResult> Hits;

			SweepForHits(StartLocation, EndLocation, Rotation, WeaponCollisionData.Shape.GetExtent(), Hits);
			
			OutHits.Append(Hits);

			WeaponCollisionData.LastLocation = EndLocation;

		}
		
		StateData.bTriggered = true;
	}
}

void AWeaponVisual::CleanupAttackData()
{
	CachedHitActors.Empty();

	auto CachedDamageData = CachedDamageStateData.Find(CurrentNotifyEvent.GetNotify());
	if (!CachedDamageData) { return; }
	for (FDamageStateData& DamageStateData : CachedDamageData->DamageStateDatas)
	{
		DamageStateData.bTriggered = false;
	}
	
	for (FWeaponCollisionData& Data : CollisionData)
	{
		Data.LastLocation = FVector::ZeroVector;
	}
	
	CurrentNotifyEvent = FAnimNotifyEventReference();
}

void AWeaponVisual::CacheCollisionData(bool bForceUpdate)
{
	// Dont recalculate collision data if we have done it already. Unless we want to update it forcefully
	if (!bForceUpdate && !CollisionData.IsEmpty())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(AWeaponVisual::CacheCollisionData)
	
	CollisionData.Empty();
	
	for (UPrimitiveComponent* Primitive : GetCollidingPrimitives())
	{
		auto BodyInstance = Primitive ? Primitive->GetBodyInstance() : nullptr;
		auto BodySetup = BodyInstance ? Primitive->GetBodyInstance()->GetBodySetup() : nullptr;

		if (BodySetup)
		{
			auto AggGeom = BodySetup->AggGeom;

			// We need a custom path for skeletal meshes and normal primitives as their relative transform needs to be from the bone
			auto SKM = Cast<USkeletalMeshComponent>(Primitive);
			FTransform RelativeTransform = SKM ? SKM->GetSocketTransform(BodySetup->BoneName, RTS_ParentBoneSpace) : FTransform::Identity;
			
			// We need to find the transform of the primitive relative to the root/actor so we can apply it to our cached collision shape
			FTransform RootTransform = RootComponent->GetRelativeTransform();
			FTransform PrimitiveTransform = Primitive->GetRelativeTransform().GetRelativeTransform(RootTransform);
			RelativeTransform.SetRotation(PrimitiveTransform.GetRotation() * RelativeTransform.GetRotation());
			RelativeTransform.SetLocation(RelativeTransform.GetLocation() + PrimitiveTransform.GetLocation());

			// Currently only supporting capsule shapes but this would be a spot to also support more types in the future
			for (const auto& CapsuleElements : AggGeom.SphylElems)
			{
				FVector Position = CapsuleElements.Center + RelativeTransform.GetLocation();
				FQuat Rotation =  RelativeTransform.GetRotation() * CapsuleElements.Rotation.Quaternion();
				FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleElements.Radius, (CapsuleElements.Length/2) + CapsuleElements.Radius);
				
				CollisionData.Emplace(Position, Rotation, FVector::ZeroVector, Shape);
			}
		}
	}
}

TArray<UPrimitiveComponent*> AWeaponVisual::GetCollidingPrimitives_Implementation()
{
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents(UPrimitiveComponent::StaticClass(), Primitives);

	return Primitives;
}

USkeletalMeshComponent* AWeaponVisual::GetRelevantWeaponMesh() const
{
	if (CharacterOwner && CharacterOwner->IsCameraInsideHead())
	{
		return LocalWeaponMesh;
	}

	return WeaponMesh;
}

TArray<USkeletalMeshComponent*> AWeaponVisual::GetWeaponMeshes() const
{
	return {WeaponMesh, LocalWeaponMesh};
}

void AWeaponVisual::UpdateWeaponAttachment()
{

}

void AWeaponVisual::ApplyAttachState()
{
}

void AWeaponVisual::OnRep_AttachState()
{
	if (AttachState.CanAttach())
	{
		if (ANarrativeCharacter* CharOwner = Cast<ANarrativeCharacter>(AttachState.CharOwner))
		{
			CharacterOwner = CharOwner;

			FString RoleStr = HasAuthority() ? "Server" : "Client";
			FString LocalStr = CharacterOwner->IsLocallyControlled() ? "Local" : "Remote";
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s: OnRep_AttachState for %s character %s"), *RoleStr, *LocalStr, *CharacterOwner->GetCharacterName().ToString());
			
			if (ANarrativeCharacterVisual* CharVisual = AttachState.VisualOwner)
			{
				if (AttachState.WeaponOwner)
				{
					VisualOwner = AttachState.VisualOwner;
					WeaponOwner = AttachState.WeaponOwner;
					
					CharVisual->SpawnedWeaponVisuals.Add(AttachState.EquippedSlot, this);
					
					UE_LOG(LogNarrativeNet, Warning, TEXT("%s: Successfully attached weapon %s for %s character %s"), *RoleStr, *GetNameSafe(this), *LocalStr, *CharacterOwner->GetCharacterName().ToString());

					//Weapon has repped, lets attach it. This could be cleaned up somewhat, we do call this more than required to be safe though that may lead
					//to unexpected behavior for people overriding virtual funcs. 
					CharVisual->AttachWeaponVisual(AttachState.WeaponOwner, AttachState.EquippedSlot, AttachState.WieldedSlot);

					//Ask our wields to re-apply since the anims may have failed due to weaponvisuals not existing yet. TODO we are spamming this func more than needed
					CharOwner->OnRep_WieldState(CharOwner->GetWeaponWieldState());

					//Allow BP/Children to do any init now that we're nicely attached.
					if (!bAttachedSuccesfully)
					{
						HandleAttachedToOwner();

						//Any attachments may have also failed to attach due to rep order - add now if so. 
						TArray<UWeaponAttachmentItem*> Attachments;
						AttachState.WeaponOwner->WeaponAttachments.GenerateValueArray(Attachments);
					
						for (auto& Attachment : Attachments)
						{
							//Any attachment without a mesh comp has failed due to missing weapon visual.  
							if (Attachment && Attachment->WeaponAttachmentSlot.IsValid() && !AttachmentMeshComps.Contains(Attachment->WeaponAttachmentSlot))
							{
								AttachState.WeaponOwner->AddAttachmentVisual(Attachment);
							}
						}
					
						bAttachedSuccesfully = true; 
					}

				}
				else
				{
					UE_LOG(LogNarrativeNet, Warning, TEXT("%s: failed to set spawned visual as weapon owner item invalid"), *RoleStr);
				}
			}
			
			//Non local players dont need this mesh - TODO remove local mesh as default component and add CreateLocalWeaponMesh() to spawn this 
			if ((!CharOwner->IsLocallyControlled() || CharOwner->IsBotControlled()) && LocalWeaponMesh)
			{

				for (auto& LocalMeshKVP : LocalAttachmentMeshComps)
				{
					if (IsValid(LocalMeshKVP.Value))
					{
						LocalMeshKVP.Value->DestroyComponent();
					}
				}

				TArray<USceneComponent*> ChildrenComps = LocalWeaponMesh->GetAttachChildren();
				LocalWeaponMesh->GetChildrenComponents(true, ChildrenComps);


				for (auto& LocalMeshChild : ChildrenComps)
				{
					if (IsValid(LocalMeshChild))
					{
						LocalMeshChild->DestroyComponent();
					}
				}

				LocalWeaponMesh->DestroyComponent();
				LocalWeaponMesh = nullptr; 
			}

			//Weapons Anim Instance may not have been able to bind to our ASC due it not being available, so sort that out. 
			if (UNarrativeAnimInstance* AnimInst = Cast<UNarrativeAnimInstance>(WeaponMesh->GetAnimInstance()))
			{
				AnimInst->BindASC();
			}

			if (LocalWeaponMesh)
			{
				if (UNarrativeAnimInstance* AnimInst = Cast<UNarrativeAnimInstance>(LocalWeaponMesh->GetAnimInstance()))
				{
					AnimInst->BindASC();
				}
			}
			
		}
		else
		{
			FString RoleStr = HasAuthority() ? "Server" : "Client";
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s: failed to set spawned visual as owner invalid"), *RoleStr);
		}
	}
	else
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s: OnRep_AttachState failed for %s as not valid attach state yet. "), *RoleStr, *GetNameSafe(this));
	}
}

void AWeaponVisual::OnRep_WeaponOwner()
{
	UpdateWeaponAttachment();
}

void AWeaponVisual::OnRep_VisualOwner()
{
	UpdateWeaponAttachment();
}

void AWeaponVisual::OnRep_Owner()
{
	Super::OnRep_Owner();

	//In case owner hasnt repped the first time weapon owner onreps. We can't guarantee replication order. 
	UpdateWeaponAttachment();
}
